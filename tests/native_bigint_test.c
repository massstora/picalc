#include "../src/native_bigint.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void require(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "native_bigint_test: %s\n", message);
        exit(1);
    }
}

static void require_decimal(const picalc_bigint *x, const char *expected) {
    char *actual = bigint_to_decimal(x);
    require(actual != NULL, "decimal conversion allocation failed");
    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "expected %s, got %s\n", expected, actual);
        free(actual);
        exit(1);
    }
    free(actual);
}

int main(void) {
    picalc_bigint a;
    picalc_bigint b;
    picalc_bigint c;
    picalc_bigint d;

    bigint_init(&a);
    bigint_init(&b);
    bigint_init(&c);
    bigint_init(&d);

    bigint_set_u64(&a, UINT64_MAX);
    bigint_set_u64(&b, 1);
    require(bigint_add(&c, &a, &b) == 0, "add failed");
    require(c.len == 2 && c.limbs[0] == 0 && c.limbs[1] == 1, "carry add mismatch");
    require_decimal(&c, "18446744073709551616");

    require(bigint_sub(&d, &c, &b) == 0, "sub failed");
    require_decimal(&d, "18446744073709551615");

    bigint_set_u64(&a, 123456789);
    bigint_set_u64(&b, 987654321);
    require(bigint_mul(&c, &a, &b) == 0, "mul failed");
    require_decimal(&c, "121932631112635269");

    require(bigint_mul_u64(&d, &c, 1000000000ULL) == 0, "mul_u64 failed");
    require_decimal(&d, "121932631112635269000000000");

    uint32_t rem = bigint_div_u32(&c, &d, 1000000000U);
    require(rem == 0, "div_u32 remainder mismatch");
    require_decimal(&c, "121932631112635269");

    bigint_clear(&a);
    bigint_clear(&b);
    bigint_clear(&c);
    bigint_clear(&d);

    puts("native bigint ok");
    return 0;
}
