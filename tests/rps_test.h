#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ASSERT_TRUE(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "%s:%d: assertion failed: %s\n", __FILE__, __LINE__, #expr); \
        exit(1); \
    } \
} while (0)

#define ASSERT_EQ_INT(expected, actual) do { \
    int e_ = (expected); \
    int a_ = (actual); \
    if (e_ != a_) { \
        fprintf(stderr, "%s:%d: expected %d, got %d\n", __FILE__, __LINE__, e_, a_); \
        exit(1); \
    } \
} while (0)

#define ASSERT_STREQ(expected, actual) do { \
    const char *e_ = (expected); \
    const char *a_ = (actual); \
    if (strcmp(e_, a_) != 0) { \
        fprintf(stderr, "%s:%d: expected \"%s\", got \"%s\"\n", __FILE__, __LINE__, e_, a_); \
        exit(1); \
    } \
} while (0)
