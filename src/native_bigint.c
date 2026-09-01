#include "native_bigint.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    DECIMAL_BASE = 1000000000U,
    DECIMAL_DIGITS = 9
};

static void bigint_normalize(picalc_bigint *x) {
    while (x->len > 0 && x->limbs[x->len - 1] == 0) {
        x->len--;
    }
}

void bigint_init(picalc_bigint *x) {
    x->len = 0;
    x->cap = 0;
    x->limbs = NULL;
}

void bigint_clear(picalc_bigint *x) {
    free(x->limbs);
    x->len = 0;
    x->cap = 0;
    x->limbs = NULL;
}

void bigint_swap(picalc_bigint *a, picalc_bigint *b) {
    picalc_bigint tmp = *a;
    *a = *b;
    *b = tmp;
}

int bigint_reserve(picalc_bigint *x, size_t cap) {
    if (cap <= x->cap) {
        return 0;
    }

    uint64_t *limbs = realloc(x->limbs, cap * sizeof(*limbs));
    if (limbs == NULL) {
        return -1;
    }

    x->limbs = limbs;
    x->cap = cap;
    return 0;
}

void bigint_set_u64(picalc_bigint *x, uint64_t value) {
    if (value == 0) {
        x->len = 0;
        return;
    }

    if (bigint_reserve(x, 1) != 0) {
        abort();
    }
    x->limbs[0] = value;
    x->len = 1;
}

int bigint_copy(picalc_bigint *dst, const picalc_bigint *src) {
    if (bigint_reserve(dst, src->len) != 0) {
        return -1;
    }
    memcpy(dst->limbs, src->limbs, src->len * sizeof(*src->limbs));
    dst->len = src->len;
    return 0;
}

int bigint_is_zero(const picalc_bigint *x) {
    return x->len == 0;
}

int bigint_cmp(const picalc_bigint *a, const picalc_bigint *b) {
    if (a->len != b->len) {
        return a->len < b->len ? -1 : 1;
    }
    for (size_t i = a->len; i > 0; i--) {
        uint64_t av = a->limbs[i - 1];
        uint64_t bv = b->limbs[i - 1];
        if (av != bv) {
            return av < bv ? -1 : 1;
        }
    }
    return 0;
}

int bigint_add(picalc_bigint *out, const picalc_bigint *a, const picalc_bigint *b) {
    const picalc_bigint *larger = a->len >= b->len ? a : b;
    const picalc_bigint *smaller = a->len >= b->len ? b : a;

    if (bigint_reserve(out, larger->len + 1) != 0) {
        return -1;
    }

    unsigned __int128 carry = 0;
    size_t i = 0;
    for (; i < smaller->len; i++) {
        unsigned __int128 sum = (unsigned __int128)larger->limbs[i] + smaller->limbs[i] + carry;
        out->limbs[i] = (uint64_t)sum;
        carry = sum >> 64;
    }
    for (; i < larger->len; i++) {
        unsigned __int128 sum = (unsigned __int128)larger->limbs[i] + carry;
        out->limbs[i] = (uint64_t)sum;
        carry = sum >> 64;
    }
    if (carry != 0) {
        out->limbs[i++] = (uint64_t)carry;
    }
    out->len = i;
    return 0;
}

int bigint_sub(picalc_bigint *out, const picalc_bigint *a, const picalc_bigint *b) {
    if (bigint_cmp(a, b) < 0 || bigint_reserve(out, a->len) != 0) {
        return -1;
    }

    uint64_t borrow = 0;
    for (size_t i = 0; i < a->len; i++) {
        uint64_t bv = i < b->len ? b->limbs[i] : 0;
        uint64_t subtrahend = bv + borrow;
        uint64_t next_borrow = subtrahend < bv || a->limbs[i] < subtrahend;
        out->limbs[i] = a->limbs[i] - subtrahend;
        borrow = next_borrow;
    }
    out->len = a->len;
    bigint_normalize(out);
    return 0;
}

int bigint_mul_u64(picalc_bigint *out, const picalc_bigint *a, uint64_t b) {
    if (a->len == 0 || b == 0) {
        out->len = 0;
        return 0;
    }
    if (bigint_reserve(out, a->len + 1) != 0) {
        return -1;
    }

    unsigned __int128 carry = 0;
    for (size_t i = 0; i < a->len; i++) {
        unsigned __int128 product = (unsigned __int128)a->limbs[i] * b + carry;
        out->limbs[i] = (uint64_t)product;
        carry = product >> 64;
    }
    out->len = a->len;
    if (carry != 0) {
        out->limbs[out->len++] = (uint64_t)carry;
    }
    return 0;
}

int bigint_add_u64(picalc_bigint *out, const picalc_bigint *a, uint64_t b) {
    picalc_bigint tmp;
    bigint_init(&tmp);
    bigint_set_u64(&tmp, b);
    int rc = bigint_add(out, a, &tmp);
    bigint_clear(&tmp);
    return rc;
}

int bigint_mul(picalc_bigint *out, const picalc_bigint *a, const picalc_bigint *b) {
    if (a->len == 0 || b->len == 0) {
        out->len = 0;
        return 0;
    }

    size_t out_len = a->len + b->len;
    if (bigint_reserve(out, out_len) != 0) {
        return -1;
    }
    memset(out->limbs, 0, out_len * sizeof(*out->limbs));

    for (size_t i = 0; i < a->len; i++) {
        unsigned __int128 carry = 0;
        for (size_t j = 0; j < b->len; j++) {
            unsigned __int128 sum =
                (unsigned __int128)a->limbs[i] * b->limbs[j] + out->limbs[i + j] + carry;
            out->limbs[i + j] = (uint64_t)sum;
            carry = sum >> 64;
        }

        size_t k = i + b->len;
        while (carry != 0) {
            unsigned __int128 sum = (unsigned __int128)out->limbs[k] + carry;
            out->limbs[k] = (uint64_t)sum;
            carry = sum >> 64;
            k++;
        }
    }

    out->len = out_len;
    bigint_normalize(out);
    return 0;
}

uint32_t bigint_div_u32(picalc_bigint *q, const picalc_bigint *a, uint32_t divisor) {
    if (divisor == 0) {
        abort();
    }
    if (bigint_reserve(q, a->len) != 0) {
        abort();
    }

    uint64_t rem = 0;
    for (size_t i = a->len; i > 0; i--) {
        unsigned __int128 cur = ((unsigned __int128)rem << 64) | a->limbs[i - 1];
        q->limbs[i - 1] = (uint64_t)(cur / divisor);
        rem = (uint64_t)(cur % divisor);
    }
    q->len = a->len;
    bigint_normalize(q);
    return (uint32_t)rem;
}

char *bigint_to_decimal(const picalc_bigint *x) {
    if (x->len == 0) {
        char *zero = malloc(2);
        if (zero != NULL) {
            zero[0] = '0';
            zero[1] = '\0';
        }
        return zero;
    }

    picalc_bigint n;
    picalc_bigint q;
    bigint_init(&n);
    bigint_init(&q);
    if (bigint_copy(&n, x) != 0) {
        bigint_clear(&n);
        bigint_clear(&q);
        return NULL;
    }

    size_t cap = x->len * 20 / DECIMAL_DIGITS + 2;
    uint32_t *chunks = malloc(cap * sizeof(*chunks));
    if (chunks == NULL) {
        bigint_clear(&n);
        bigint_clear(&q);
        return NULL;
    }

    size_t count = 0;
    while (!bigint_is_zero(&n)) {
        if (count == cap) {
            size_t next_cap = cap * 2;
            uint32_t *next = realloc(chunks, next_cap * sizeof(*next));
            if (next == NULL) {
                free(chunks);
                bigint_clear(&n);
                bigint_clear(&q);
                return NULL;
            }
            chunks = next;
            cap = next_cap;
        }
        chunks[count++] = bigint_div_u32(&q, &n, DECIMAL_BASE);
        bigint_swap(&n, &q);
    }

    size_t chars = 11 + (count - 1) * DECIMAL_DIGITS + 1;
    char *out = malloc(chars);
    if (out == NULL) {
        free(chunks);
        bigint_clear(&n);
        bigint_clear(&q);
        return NULL;
    }

    char *p = out;
    p += sprintf(p, "%u", chunks[count - 1]);
    for (size_t i = count - 1; i > 0; i--) {
        p += sprintf(p, "%09u", chunks[i - 1]);
    }
    *p = '\0';

    free(chunks);
    bigint_clear(&n);
    bigint_clear(&q);
    return out;
}
