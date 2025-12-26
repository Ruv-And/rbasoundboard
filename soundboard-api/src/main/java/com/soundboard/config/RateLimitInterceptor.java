package com.soundboard.config;

import lombok.extern.slf4j.Slf4j;
import org.springframework.stereotype.Component;
import org.springframework.web.servlet.HandlerInterceptor;

import jakarta.servlet.http.HttpServletRequest;
import jakarta.servlet.http.HttpServletResponse;

/**
 * HTTP interceptor that enforces rate limiting on all API requests.
 * Returns 429 (Too Many Requests) if rate limit is exceeded.
 */
@Component
@Slf4j
public class RateLimitInterceptor implements HandlerInterceptor {

    @Override
    public boolean preHandle(HttpServletRequest request, HttpServletResponse response, Object handler)
            throws Exception {

        // Extract client IP (supports X-Forwarded-For header for proxies/load
        // balancers)
        String clientIp = getClientIp(request);

        // Check rate limit
        if (!RateLimitConfig.allowRequest(clientIp)) {
            log.warn("Rate limit exceeded for client: {}", clientIp);

            response.setStatus(429); // SC_TOO_MANY_REQUESTS
            response.setContentType("application/json");
            response.getWriter().write("{\"error\": \"Rate limit exceeded. Maximum 5 requests per second.\"}");
            return false;
        }

        // Add rate limit info to response headers
        response.addHeader("X-RateLimit-Limit", String.valueOf(RateLimitConfig.getLimitCapacity()));
        response.addHeader("X-RateLimit-Window", RateLimitConfig.getWindowDescription());
        response.addHeader("X-RateLimit-Remaining", String.valueOf(RateLimitConfig.getAvailableTokens(clientIp)));

        return true;
    }

    /**
     * Extract the real client IP, accounting for proxies and load balancers.
     * Checks X-Forwarded-For first, then falls back to direct IP.
     */
    private String getClientIp(HttpServletRequest request) {
        // Check for X-Forwarded-For header (set by proxies/load balancers)
        String xForwardedFor = request.getHeader("X-Forwarded-For");
        if (xForwardedFor != null && !xForwardedFor.isEmpty()) {
            // X-Forwarded-For can contain multiple IPs; take the first one (client)
            return xForwardedFor.split(",")[0].trim();
        }

        // Check for X-Real-IP header (sometimes set by nginx)
        String xRealIp = request.getHeader("X-Real-IP");
        if (xRealIp != null && !xRealIp.isEmpty()) {
            return xRealIp;
        }

        // Fall back to remote address
        return request.getRemoteAddr();
    }
}
