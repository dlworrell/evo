/*
 * External EVO archive-audit research consumer: one-pass C17 chunk hashing.
 * SHA-256 uses OpenSSL EVP; output is provisional JSONL until the final record.
 * No writes to input, no deletion, no parity or mail parsing.
 */
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/evp.h>

#define CHUNK_DEFAULT (4U * 1024U * 1024U)
#define CHUNK_MIN 4096U
#define CHUNK_MAX (64U * 1024U * 1024U)

static void as_hex(const unsigned char *src, size_t n, char *dst) {
    static const char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < n; ++i) {
        dst[2 * i] = hex[src[i] >> 4];
        dst[2 * i + 1] = hex[src[i] & 15U];
    }
    dst[2 * n] = '\0';
}

static int parse_chunk_size(const char *s, size_t *out) {
    char *end = NULL;
    unsigned long long size;
    if (s == NULL || *s == '\0' || *s == '-') return 0;
    errno = 0;
    size = strtoull(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0' ||
        size < CHUNK_MIN || size > CHUNK_MAX) return 0;
    *out = (size_t)size;
    return 1;
}

static int hash_stream(FILE *input, size_t chunk_bytes) {
    EVP_MD_CTX *whole = NULL;
    unsigned char *buffer = NULL;
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len = 0;
    uint64_t offset = 0;
    uint64_t chunks = 0;
    int result = 1;

    buffer = malloc(chunk_bytes);
    whole = EVP_MD_CTX_new();
    if (buffer == NULL || whole == NULL ||
        EVP_DigestInit_ex(whole, EVP_sha256(), NULL) != 1) {
        fputs("ERROR: allocation or SHA-256 initialization failed\n", stderr);
        goto cleanup;
    }
    if (printf("{\"schema\":\"evo.archive-audit.stream-sha256.v1\",\"chunk_bytes\":%zu}\n",
               chunk_bytes) < 0) goto cleanup;

    for (;;) {
        size_t filled = 0;
        while (filled < chunk_bytes) {
            size_t count = fread(buffer + filled, 1, chunk_bytes - filled, input);
            if (count > 0) {
                filled += count;
            } else {
                if (ferror(input)) {
                    fputs("ERROR: source read failed\n", stderr);
                    goto cleanup;
                }
                break;
            }
        }
        if (filled == 0) break;
        if (UINT64_MAX - offset < (uint64_t)filled || chunks == UINT64_MAX) {
            fputs("ERROR: source length or chunk count overflow\n", stderr);
            goto cleanup;
        }
        if (EVP_DigestUpdate(whole, buffer, filled) != 1 ||
            EVP_Digest(buffer, filled, digest, &digest_len, EVP_sha256(), NULL) != 1 ||
            digest_len != 32U) {
            fputs("ERROR: SHA-256 update failed\n", stderr);
            goto cleanup;
        }
        char hex[65];
        as_hex(digest, digest_len, hex);
        if (printf("{\"kind\":\"chunk\",\"offset\":%" PRIu64
                   ",\"length\":%zu,\"sha256\":\"%s\"}\n",
                   offset, filled, hex) < 0) goto cleanup;
        offset += (uint64_t)filled;
        ++chunks;
    }
    if (EVP_DigestFinal_ex(whole, digest, &digest_len) != 1 || digest_len != 32U) {
        fputs("ERROR: SHA-256 finalization failed\n", stderr);
        goto cleanup;
    }
    char hex[65];
    as_hex(digest, digest_len, hex);
    if (printf("{\"kind\":\"file\",\"length\":%" PRIu64
               ",\"chunks\":%" PRIu64 ",\"sha256\":\"%s\",\"complete\":true}\n",
               offset, chunks, hex) < 0 || fflush(stdout) != 0 || ferror(stdout)) {
        fputs("ERROR: output manifest write failed\n", stderr);
        goto cleanup;
    }
    result = 0;
cleanup:
    EVP_MD_CTX_free(whole);
    free(buffer);
    return result;
}

int main(int argc, char **argv) {
    FILE *in = NULL;
    size_t chunk_bytes = CHUNK_DEFAULT;
    int status;
    if (argc < 2 || argc > 3 ||
        (argc == 3 && !parse_chunk_size(argv[2], &chunk_bytes))) {
        fprintf(stderr, "Usage: %s <input-file|-> [chunk-bytes: 4096..67108864]\n", argv[0]);
        return 2;
    }
    if (strcmp(argv[1], "-") == 0) {
        in = stdin;
    } else {
        in = fopen(argv[1], "rb");
        if (in == NULL) {
            perror("source open");
            return 2;
        }
    }
    status = hash_stream(in, chunk_bytes);
    if (in != stdin && fclose(in) != 0) {
        perror("source close");
        return 1;
    }
    return status;
}
