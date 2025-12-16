# SSL/TLS Setup for gRPC Audio Processor

## Overview

The audio processor service now supports SSL/TLS for secure gRPC communication in production environments. This guide covers certificate generation and configuration.

## Quick Start (Development)

By default, the service runs in **insecure mode** for local development. No configuration needed.

## Production Setup

### 1. Generate SSL/TLS Certificates

#### Option A: Self-Signed Certificates (Testing/Internal)

```bash
# Generate CA private key
openssl genrsa -out ca.key 4096

# Generate CA certificate
openssl req -new -x509 -days 3650 -key ca.key -out ca.crt \
  -subj "/C=US/ST=State/L=City/O=Organization/CN=Audio Processor CA"

# Generate server private key
openssl genrsa -out server.key 4096

# Generate server certificate signing request
openssl req -new -key server.key -out server.csr \
  -subj "/C=US/ST=State/L=City/O=Organization/CN=audio-processor"

# Sign server certificate with CA
openssl x509 -req -days 3650 -in server.csr -CA ca.crt -CAkey ca.key \
  -set_serial 01 -out server.crt

# Optional: Generate client certificates for mutual TLS
openssl genrsa -out client.key 4096
openssl req -new -key client.key -out client.csr \
  -subj "/C=US/ST=State/L=City/O=Organization/CN=soundboard-api"
openssl x509 -req -days 3650 -in client.csr -CA ca.crt -CAkey ca.key \
  -set_serial 02 -out client.crt
```

#### Option B: Let's Encrypt (Public/Production)

For production deployments with public endpoints, use Let's Encrypt or your organization's certificate authority.

### 2. Configure C++ Audio Processor Service

Set environment variables:

```bash
# Required for TLS
export GRPC_SERVER_CERT_PATH=/path/to/server.crt
export GRPC_SERVER_KEY_PATH=/path/to/server.key

# Optional: Enable mutual TLS (client certificate verification)
export GRPC_SERVER_ROOT_CERT_PATH=/path/to/ca.crt
```

#### Docker Compose Example

```yaml
audio-processor:
  image: audio-processor-service
  environment:
    - GRPC_SERVER_CERT_PATH=/certs/server.crt
    - GRPC_SERVER_KEY_PATH=/certs/server.key
    - GRPC_SERVER_ROOT_CERT_PATH=/certs/ca.crt # Optional: mutual TLS
    - AUDIO_PROC_MAX_CONCURRENCY=8
  volumes:
    - ./certs:/certs:ro
  ports:
    - "50051:50051"
```

### 3. Configure Java Client (Spring Boot)

Add to `application.yml`:

```yaml
grpc:
  audio-processor:
    host: audio-processor
    port: 50051
    use-tls: true
    ca-cert-path: /certs/ca.crt # Required for TLS

    # Optional: Mutual TLS (client authentication)
    client-cert-path: /certs/client.crt
    client-key-path: /certs/client.key
```

#### Docker Compose Example

```yaml
soundboard-api:
  image: soundboard-api
  environment:
    - GRPC_AUDIO_PROCESSOR_USE_TLS=true
    - GRPC_AUDIO_PROCESSOR_CA_CERT_PATH=/certs/ca.crt
    - GRPC_AUDIO_PROCESSOR_CLIENT_CERT_PATH=/certs/client.crt
    - GRPC_AUDIO_PROCESSOR_CLIENT_KEY_PATH=/certs/client.key
  volumes:
    - ./certs:/certs:ro
```

## Security Best Practices

1. **Never commit certificates to version control**

   - Add `*.crt`, `*.key`, `*.pem` to `.gitignore`

2. **Use strong key sizes**

   - Minimum 2048-bit RSA (4096-bit recommended)
   - Or use ECDSA P-256 or higher

3. **Rotate certificates regularly**

   - Recommended: 90-day rotation for production

4. **Enable mutual TLS for internal services**

   - Prevents unauthorized clients from connecting

5. **Use proper file permissions**

   ```bash
   chmod 600 *.key
   chmod 644 *.crt
   ```

6. **Certificate validation**
   - Ensure CN/SAN matches server hostname
   - Verify certificate expiration dates

## Verification

### Test TLS Connection (C++ Server)

```bash
# Should show SSL handshake
openssl s_client -connect localhost:50051 -CAfile ca.crt

# With client cert (mutual TLS)
openssl s_client -connect localhost:50051 \
  -CAfile ca.crt \
  -cert client.crt \
  -key client.key
```

### Monitor Logs

**C++ Service:**

```
Loading SSL/TLS credentials for secure gRPC...
✓ SSL/TLS credentials configured
Async Audio Processor Server listening on 0.0.0.0:50051
```

**Java Client:**

```
Loaded CA certificate: /certs/ca.crt
✓ SSL/TLS channel established
```

## Troubleshooting

### "SSL configuration error"

- Check certificate file paths exist and are readable
- Verify file permissions (keys should be 600)

### "certificate verify failed"

- Ensure CA certificate matches the server certificate chain
- Check certificate expiration dates
- Verify hostname/CN matches

### "handshake failure"

- Protocol version mismatch (both sides must support TLS 1.2+)
- Cipher suite compatibility issues
- Check firewall rules

### Performance Impact

- TLS adds ~1-2ms latency per RPC
- Minimal CPU overhead with modern hardware (AES-NI)
- No impact on throughput for streaming operations

## Migration from Insecure to Secure

1. Deploy with TLS disabled (current state)
2. Add certificates to both services (volumes/secrets)
3. Enable TLS on C++ server (environment variables)
4. Enable TLS on Java client (application.yml)
5. Restart services in order: C++ → Java
6. Verify logs show "SSL/TLS" messages
7. Test end-to-end upload and streaming

## References

- [gRPC Authentication Guide](https://grpc.io/docs/guides/auth/)
- [gRPC C++ Security](https://grpc.io/docs/languages/cpp/basics/#authentication)
- [gRPC Java Security](https://grpc.io/docs/languages/java/basics/#tls)
