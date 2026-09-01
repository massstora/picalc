#ifndef PICALC_NATIVE_BIGINT_H
#define PICALC_NATIVE_BIGINT_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    size_t len;
    size_t cap;
    uint64_t *limbs;
} picalc_bigint;

void bigint_init(picalc_bigint *x);
void bigint_clear(picalc_bigint *x);
void bigint_swap(picalc_bigint *a, picalc_bigint *b);
int bigint_reserve(picalc_bigint *x, size_t cap);
void bigint_set_u64(picalc_bigint *x, uint64_t value);
int bigint_copy(picalc_bigint *dst, const picalc_bigint *src);
int bigint_is_zero(const picalc_bigint *x);
int bigint_cmp(const picalc_bigint *a, const picalc_bigint *b);
int bigint_add(picalc_bigint *out, const picalc_bigint *a, const picalc_bigint *b);
int bigint_sub(picalc_bigint *out, const picalc_bigint *a, const picalc_bigint *b);
int bigint_mul_u64(picalc_bigint *out, const picalc_bigint *a, uint64_t b);
int bigint_add_u64(picalc_bigint *out, const picalc_bigint *a, uint64_t b);
int bigint_mul(picalc_bigint *out, const picalc_bigint *a, const picalc_bigint *b);
uint32_t bigint_div_u32(picalc_bigint *q, const picalc_bigint *a, uint32_t divisor);
char *bigint_to_decimal(const picalc_bigint *x);

#endif
