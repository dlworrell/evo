/*
 * Read-only external archive-audit C17 research tool.
 *
 * SHA-256 of a whole stream and its fixed-size byte chunks in ONE input pass.
 * Uses OpenSSL EVP and a bounded 64-KiB automatic I/O buffer, regardless of
 * chunk size. Record output is PROVISIONAL until the final complete marker.
 * It does not parse MBOX, handle transport, repair data or authorize deletion.
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
#define IO_BYTES 65536U

static void as_hex(const unsigned char *src, size_t n, char *dst) {
    static const char digits[] = "0123456789abcdef";
    for (size_t i = 0; i < n; ++i) {
        dst[2 * i] = digits[src[i] >> 4];
        dst[2 * i + 1] = digits[src[i] & 15U];
    }
    dst[2 * n] = '\0';
}

static int parse_chunk_size(const char *arg, size_t *size) {
    char *end = NULL;
    unsigned long long n;
    if (arg == NULL || *arg == '\0' || *arg == '-') return 0;
    errno = 0;
    n = strtoull(arg, &end, 10);
    if (errno != 0 || end == arg || *end != '\0' ||
        n < CHUNK_MIN || n > CHUNK_MAX) return 0;
    *size = (size_t)n; /* n has been bounded by CHUNK_MAX. */
    return 1;
}

static int hash_stream(FILE *input, size_t chunk_bytes) {
    /*
     * Local bounded storage: no dynamically allocated archive-sized buffers.
     * EVP owns and releases its internal state via the matching EVP functions.
     */
    unsigned char buffer[IO_BYTES];
    unsigned char digest[EVP_MAX_MD_SIZE];
    char hex[65];
    unsigned int digest_len = 0U;
    EVP_MD_CTX *whole = EVP_MD_CTX_new();
    EVP_MD_CTX *chunk = EVP_MD_CTX_new();
    uint64_t offset = 0U;
    uint64_t chunks = 0U;
    int status = 1;
    int at_end = 0;
    if (whole == NULL || chunk == NULL ||
        EVP_DigestInit_ex(whole, EVP_sha256(), NULL) != 1) {
        fputs("ERROR: SHA-256 context initialization failed\n", stderr);
        goto done;
    }
    if (printf("{\"schema\":\"evo.archive-audit.stream-sha256.v1\","
               "\"chunk_bytes\":%zu}\n", chunk_bytes) < 0) goto done;
    while (!at_end) {
        size_t filled = 0;
        if (EVP_DigestInit_ex(chunk, EVP_sha256(), NULL) != 1) {
            fputs("ERROR: chunk SHA-256 initialization failed\n", stderr);
            goto done;
        }
        while (filled < chunk_bytes) {
            size_t want = chunk_bytes - filled;
            if (want > sizeof buffer) want = sizeof buffer;
            size_t got = fread(buffer, 1U, want, input);
            if (got == 0U) {
                if (ferror(input)) {
                    fputs("ERROR: input read failure\n", stderr);
                    goto done;
                }
                at_end = 1;
                break;
            }
            if (UINT64_MAX - offset < (uint64_t)filled + (uint64_t)got) {
                fputs("ERROR: byte-offset overflow\n", stderr);
                goto done;
            }
            if (EVP_DigestUpdate(chunk, buffer, got) != 1 ||
                EVP_DigestUpdate(whole, buffer, got) != 1) {
                fputs("ERROR: SHA-256 update failed\n", stderr);
                goto done;
            }
            filled += got;
        }
        if (filled == 0U) break;
        if (chunks == UINT64_MAX) {
            fputs("ERROR: chunk-count overflow\n", stderr);
            goto done;
        }
        if (EVP_DigestFinal_ex(chunk, digest, &digest_len) != 1 ||
            digest_len != 32U) {
            fputs("ERROR: chunk SHA-256 finalization failed\n", stderr);
            goto done;
        }
        as_hex(digest, digest_len, hex);
        if (printf("{\"kind\":\"chunk\",\"offset\":%" PRIu64
                   ",\"length\":%zu,\"sha256\":\"%s\"}\n",
                   offset, filled, hex) < 0) {
            fputs("ERROR: manifest write failed\n", stderr);
            goto done;
        }
        offset += (uint64_t)filled;
        ++chunks;
    }
    if (EVP_DigestFinal_ex(whole, digest, &digest_len) != 1 ||
        digest_len != 32U) {
        fputs("ERROR: file SHA-256 finalization failed\n", stderr);
        goto done;
    }
    as_hex(digest, digest_len, hex);
    if (printf("{\"kind\":\"file\",\"length\":%" PRIu64
               ",\"chunks\":%" PRIu64
               ",\"sha256\":\"%s\",\"complete\":true}\n",
               offset, chunks, hex) < 0 || fflush(stdout) != 0 ||
        ferror(stdout)) {
        fputs("ERROR: final manifest write failed\n", stderr);
        goto done;
    }
    status = 0;
done:
    EVP_MD_CTX_free(chunk);
    EVP_MD_CTX_free(whole);
    return status;
}

int main(int argc, char **argv) {
    size_t chunk_bytes = CHUNK_DEFAULT;
    FILE *input;
    int status;
    if (argc < 2 || argc > 3 ||
        (argc == 3 && !parse_chunk_size(argv[2], &chunk_bytes))) {
        fprintf(stderr,
                "Usage: %s <input-file|-> [chunk-bytes: 4096..67108864]\n",
                argv[0]);
        return 2;
    }
    if (strcmp(argv[1], "-") == 0) {
        input = stdin;
    } else {
        input = fopen(argv[1], "rb");
        if (input == NULL) {
            perror("source open");
            return 2;
        }
    }
    status = hash_stream(input, chunk_bytes);
    if (input != stdin && fclose(input) != 0) {
        perror("source close");
        return 1;
    }
    return status;
}
