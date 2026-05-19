#include "rps/sha256.h"
#include "rps_test.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void digest_to_hex(const uint8_t digest[32], char out[65]) {
    size_t i;
    for (i = 0; i < 32; ++i) sprintf(out + i * 2, "%02x", digest[i]);
    out[64] = '\0';
}

int main(void) {
    uint8_t digest[32];
    char hex[65];
    const char *payload = "alice:0:a1";

    rps_sha256((const uint8_t *)payload, strlen(payload), digest);
    digest_to_hex(digest, hex);

    ASSERT_STREQ("4d936656be1127ebc333b0cc1fa36668cda69b303bc21501127c7196ff670333", hex);
    return 0;
}
