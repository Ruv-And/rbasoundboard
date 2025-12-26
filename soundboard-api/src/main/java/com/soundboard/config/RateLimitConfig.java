package com.soundboard.config;

import io.github.bucket4j.Bandwidth;
import io.github.bucket4j.Bucket;
import io.github.bucket4j.BucketConfiguration;
import io.github.bucket4j.distributed.proxy.ProxyManager;
import io.github.bucket4j.local.LocalBucketBuilder;
import org.springframework.context.annotation.Configuration;

import java.time.Duration;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

/**
 * Rate limiting configuration using Bucket4j.
 * Supports either per-IP buckets or a single global bucket.
 * Defaults: 5 requests/second, scope = per-IP.
 *
 * Configure via env vars:
 * - RATE_LIMIT_RPS (int, default 5)
 * - RATE_LIMIT_SCOPE ("ip" | "global", default "ip")
 */
@Configuration
public class RateLimitConfig {

    private static final int RPS = resolveIntEnv("RATE_LIMIT_RPS", 5);
    private static final String SCOPE = resolveEnv("RATE_LIMIT_SCOPE", "ip").toLowerCase();

    // Token bucket definition: RPS tokens refilled every 1s
    private static final Bandwidth LIMIT = Bandwidth.builder()
            .capacity(RPS)
            .refillGreedy(RPS, Duration.ofSeconds(1))
            .build();

    // Global bucket (used when SCOPE == "global")
    private static final Bucket GLOBAL_BUCKET = Bucket.builder()
            .addLimit(LIMIT)
            .build();

    // Per-IP buckets cache (used when SCOPE == "ip")
    private static final Map<String, Bucket> BUCKETS = new ConcurrentHashMap<>();

    public static Bucket resolveBucket(String clientIp) {
        if ("global".equals(SCOPE)) {
            return GLOBAL_BUCKET;
        }
        // default: per-IP buckets
        return BUCKETS.computeIfAbsent(clientIp, ip -> Bucket.builder()
                .addLimit(LIMIT)
                .build());
    }

    /**
     * Try to consume one token; returns true if allowed.
     */
    public static boolean allowRequest(String clientIp) {
        return resolveBucket(clientIp).tryConsume(1);
    }

    /**
     * Remaining tokens in the current window for the resolved bucket.
     */
    public static long getAvailableTokens(String clientIp) {
        return resolveBucket(clientIp).getAvailableTokens();
    }

    /** Limit capacity (tokens per second). */
    public static int getLimitCapacity() {
        return RPS;
    }

    /** Window description for headers. */
    public static String getWindowDescription() {
        return "1s";
    }

    private static String resolveEnv(String name, String def) {
        String v = System.getenv(name);
        return (v == null || v.isBlank()) ? def : v;
    }

    private static int resolveIntEnv(String name, int def) {
        String v = System.getenv(name);
        if (v == null || v.isBlank())
            return def;
        try {
            int parsed = Integer.parseInt(v.trim());
            return parsed > 0 ? parsed : def;
        } catch (Exception e) {
            return def;
        }
    }
}
