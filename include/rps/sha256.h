#pragma once

#include <stddef.h>
#include <stdint.h>

void rps_sha256(const uint8_t *data, size_t len, uint8_t hash[32]);
