#define _POSIX_C_SOURCE 200809L

#include "chudnovsky.h"

#include <dirent.h>
#include <errno.h>
#include <gmp.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum {
    CHUDNOVSKY_DIGITS_PER_TERM = 14,
    GUARD_DIGITS = 20,
    MIN_TERMS_PER_THREAD = 256
};

static const unsigned long C3_OVER_24 = 10939058860032000UL;

static void usage(FILE *stream) {
    fputs("usage: ./picalc DIGITS [-o FILE]\n", stream);
}

struct core_id {
    long package_id;
    long core_id;
};

struct split_result {
    mpz_t P;
    mpz_t Q;
    mpz_t T;
};

struct split_task {
    unsigned long start;
    unsigned long end;
    struct split_result *result;
};

static int parse_digits(const char *text, unsigned long *digits) {
    char *end = NULL;

    errno = 0;
    unsigned long value = strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0') {
        return -1;
    }

    *digits = value;
    return 0;
}

static int parse_args(int argc, char **argv, unsigned long *digits, const char **output_path) {
    if (argc != 2 && argc != 4) {
        return -1;
    }

    if (parse_digits(argv[1], digits) != 0) {
        return -1;
    }

    *output_path = NULL;
    if (argc == 4) {
        if (strcmp(argv[2], "-o") != 0 || argv[3][0] == '\0') {
            return -1;
        }
        *output_path = argv[3];
    }

    return 0;
}

static int checked_add_ul(unsigned long a, unsigned long b, unsigned long *out) {
    if (ULONG_MAX - a < b) {
        return -1;
    }
    *out = a + b;
    return 0;
}

static int checked_terms_for_digits(unsigned long digits, unsigned long *terms) {
    unsigned long precision_digits = 0;
    if (checked_add_ul(digits, GUARD_DIGITS, &precision_digits) != 0) {
        return -1;
    }

    *terms = precision_digits / CHUDNOVSKY_DIGITS_PER_TERM + 2;
    if (*terms == 0) {
        return -1;
    }

    return 0;
}

static void split_result_init(struct split_result *result) {
    mpz_inits(result->P, result->Q, result->T, NULL);
}

static void split_result_clear(struct split_result *result) {
    mpz_clears(result->P, result->Q, result->T, NULL);
}

static int parse_long_file(const char *path, long *value) {
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        return -1;
    }

    int scanned = fscanf(file, "%ld", value);
    fclose(file);
    return scanned == 1 ? 0 : -1;
}

static int has_core_id(const struct core_id *cores, size_t count, long package_id, long core_id) {
    for (size_t i = 0; i < count; i++) {
        if (cores[i].package_id == package_id && cores[i].core_id == core_id) {
            return 1;
        }
    }
    return 0;
}

static unsigned int detect_physical_cores(void) {
    DIR *dir = opendir("/sys/devices/system/cpu");
    if (dir == NULL) {
        return 1;
    }

    size_t capacity = 16;
    size_t count = 0;
    struct core_id *cores = calloc(capacity, sizeof(*cores));
    if (cores == NULL) {
        closedir(dir);
        return 1;
    }

    struct dirent *entry = NULL;
    while ((entry = readdir(dir)) != NULL) {
        char *end = NULL;
        if (strncmp(entry->d_name, "cpu", 3) != 0) {
            continue;
        }

        errno = 0;
        unsigned long cpu = strtoul(entry->d_name + 3, &end, 10);
        if (errno != 0 || end == entry->d_name + 3 || *end != '\0') {
            continue;
        }

        char package_path[128];
        char core_path[128];
        long package_id = 0;
        long core_id = 0;

        snprintf(package_path, sizeof(package_path),
                 "/sys/devices/system/cpu/cpu%lu/topology/physical_package_id", cpu);
        snprintf(core_path, sizeof(core_path),
                 "/sys/devices/system/cpu/cpu%lu/topology/core_id", cpu);

        if (parse_long_file(package_path, &package_id) != 0 ||
            parse_long_file(core_path, &core_id) != 0 ||
            has_core_id(cores, count, package_id, core_id)) {
            continue;
        }

        if (count == capacity) {
            size_t next_capacity = capacity * 2;
            struct core_id *next = realloc(cores, next_capacity * sizeof(*next));
            if (next == NULL) {
                free(cores);
                closedir(dir);
                return 1;
            }
            cores = next;
            capacity = next_capacity;
        }

        cores[count].package_id = package_id;
        cores[count].core_id = core_id;
        count++;
    }

    free(cores);
    closedir(dir);

    if (count == 0 || count > UINT_MAX) {
        return 1;
    }
    return (unsigned int)count;
}

/*
 * Binary splitting for the Chudnovsky series:
 *
 *   P(a,b), Q(a,b), and T(a,b) combine so that T / Q is the partial sum.
 *   pi = Q * 426880 * sqrt(10005) / T
 */
static void binary_split(unsigned long a, unsigned long b, mpz_t P, mpz_t Q, mpz_t T) {
    if (b - a == 1) {
        if (a == 0) {
            mpz_set_ui(P, 1);
            mpz_set_ui(Q, 1);
            mpz_set_ui(T, 13591409);
            return;
        }

        mpz_set_ui(P, 6 * a - 5);
        mpz_mul_ui(P, P, 2 * a - 1);
        mpz_mul_ui(P, P, 6 * a - 1);

        mpz_set_ui(Q, a);
        mpz_mul_ui(Q, Q, a);
        mpz_mul_ui(Q, Q, a);
        mpz_mul_ui(Q, Q, C3_OVER_24);

        mpz_set(T, P);
        mpz_mul_ui(T, T, 13591409 + 545140134 * a);
        if ((a & 1UL) != 0) {
            mpz_neg(T, T);
        }
        return;
    }

    unsigned long m = a + (b - a) / 2;
    mpz_t P1, Q1, T1, P2, Q2, T2, tmp;

    mpz_inits(P1, Q1, T1, P2, Q2, T2, tmp, NULL);
    binary_split(a, m, P1, Q1, T1);
    binary_split(m, b, P2, Q2, T2);

    mpz_mul(P, P1, P2);
    mpz_mul(Q, Q1, Q2);

    mpz_mul(T, T1, Q2);
    mpz_mul(tmp, P1, T2);
    mpz_add(T, T, tmp);

    mpz_clears(P1, Q1, T1, P2, Q2, T2, tmp, NULL);
}

static void combine_split_results(struct split_result *left, const struct split_result *right) {
    mpz_t combined_T, tmp;

    mpz_inits(combined_T, tmp, NULL);

    mpz_mul(combined_T, left->T, right->Q);
    mpz_mul(tmp, left->P, right->T);
    mpz_add(combined_T, combined_T, tmp);

    mpz_mul(left->P, left->P, right->P);
    mpz_mul(left->Q, left->Q, right->Q);
    mpz_set(left->T, combined_T);

    mpz_clears(combined_T, tmp, NULL);
}

static void *run_split_task(void *arg) {
    struct split_task *task = arg;
    binary_split(task->start, task->end, task->result->P, task->result->Q, task->result->T);
    return NULL;
}

static int binary_split_threaded(unsigned long terms, unsigned int requested_threads,
                                 mpz_t P, mpz_t Q, mpz_t T) {
    unsigned int threads = requested_threads;
    unsigned long max_useful_threads = terms / MIN_TERMS_PER_THREAD;

    if (max_useful_threads == 0) {
        max_useful_threads = 1;
    }
    if ((unsigned long)threads > max_useful_threads) {
        threads = (unsigned int)max_useful_threads;
    }
    if ((unsigned long)threads > terms) {
        threads = (unsigned int)terms;
    }
    if (threads < 2) {
        binary_split(0, terms, P, Q, T);
        return 0;
    }

    struct split_result *results = calloc(threads, sizeof(*results));
    struct split_task *tasks = calloc(threads, sizeof(*tasks));
    pthread_t *workers = calloc(threads, sizeof(*workers));
    if (results == NULL || tasks == NULL || workers == NULL) {
        free(results);
        free(tasks);
        free(workers);
        return -1;
    }

    for (unsigned int i = 0; i < threads; i++) {
        split_result_init(&results[i]);
        tasks[i].start = ((unsigned long)i * terms) / threads;
        tasks[i].end = ((unsigned long)(i + 1) * terms) / threads;
        tasks[i].result = &results[i];
    }

    unsigned int created = 0;
    for (; created < threads; created++) {
        if (pthread_create(&workers[created], NULL, run_split_task, &tasks[created]) != 0) {
            break;
        }
    }

    int failed = created != threads;
    for (unsigned int i = 0; i < created; i++) {
        if (pthread_join(workers[i], NULL) != 0) {
            failed = 1;
        }
    }

    if (!failed) {
        for (unsigned int stride = 1; stride < threads; stride *= 2) {
            for (unsigned int i = 0; i + stride < threads; i += stride * 2) {
                combine_split_results(&results[i], &results[i + stride]);
            }
        }
        mpz_set(P, results[0].P);
        mpz_set(Q, results[0].Q);
        mpz_set(T, results[0].T);
    }

    for (unsigned int i = 0; i < threads; i++) {
        split_result_clear(&results[i]);
    }
    free(results);
    free(tasks);
    free(workers);

    return failed ? -1 : 0;
}

static int write_pi(FILE *stream, const mpz_t pi_scaled, unsigned long digits) {
    char *raw = mpz_get_str(NULL, 10, pi_scaled);
    if (raw == NULL) {
        return -1;
    }

    size_t len = strlen(raw);
    if (digits == 0) {
        if (fprintf(stream, "%s\n", raw) < 0) {
            free(raw);
            return -1;
        }
        free(raw);
        return 0;
    }

    size_t requested = (size_t)digits;
    if (requested != digits) {
        free(raw);
        return -1;
    }

    size_t int_len = len > requested ? len - requested : 1;
    if (fwrite(raw, 1, int_len, stream) != int_len || fputc('.', stream) == EOF) {
        free(raw);
        return -1;
    }

    if (len <= requested) {
        size_t zero_count = requested - len + 1;
        for (size_t i = 0; i < zero_count; i++) {
            if (fputc('0', stream) == EOF) {
                free(raw);
                return -1;
            }
        }
        if (fputs(raw, stream) == EOF) {
            free(raw);
            return -1;
        }
    } else {
        if (fwrite(raw + int_len, 1, requested, stream) != requested) {
            free(raw);
            return -1;
        }
    }

    if (fputc('\n', stream) == EOF) {
        free(raw);
        return -1;
    }

    free(raw);
    return 0;
}

static int compute_pi(mpz_t pi_scaled, unsigned long digits) {
    unsigned long scale_digits = 0;
    unsigned long terms = 0;

    if (checked_add_ul(digits, GUARD_DIGITS, &scale_digits) != 0 ||
        checked_terms_for_digits(digits, &terms) != 0) {
        return -1;
    }

    mpz_t P, Q, T, sqrt_arg, sqrt_scaled, pow10, numerator, guard_divisor;
    mpz_inits(P, Q, T, sqrt_arg, sqrt_scaled, pow10, numerator, guard_divisor, NULL);

    if (binary_split_threaded(terms, detect_physical_cores(), P, Q, T) != 0) {
        mpz_clears(P, Q, T, sqrt_arg, sqrt_scaled, pow10, numerator, guard_divisor, NULL);
        return -1;
    }

    mpz_ui_pow_ui(pow10, 10, scale_digits);
    mpz_mul(sqrt_arg, pow10, pow10);
    mpz_mul_ui(sqrt_arg, sqrt_arg, 10005);
    mpz_sqrt(sqrt_scaled, sqrt_arg);

    mpz_mul(numerator, Q, sqrt_scaled);
    mpz_mul_ui(numerator, numerator, 426880);
    mpz_tdiv_q(pi_scaled, numerator, T);

    mpz_ui_pow_ui(guard_divisor, 10, GUARD_DIGITS);
    mpz_tdiv_q(pi_scaled, pi_scaled, guard_divisor);

    mpz_clears(P, Q, T, sqrt_arg, sqrt_scaled, pow10, numerator, guard_divisor, NULL);
    return 0;
}

int picalc_run(int argc, char **argv) {
    unsigned long digits = 0;
    const char *output_path = NULL;

    if (parse_args(argc, argv, &digits, &output_path) != 0) {
        usage(stderr);
        return 2;
    }

    mpz_t pi_scaled;
    mpz_init(pi_scaled);

    if (compute_pi(pi_scaled, digits) != 0) {
        fputs("picalc: requested precision is too large for this build\n", stderr);
        mpz_clear(pi_scaled);
        return 1;
    }

    FILE *stream = stdout;
    if (output_path != NULL) {
        stream = fopen(output_path, "w");
        if (stream == NULL) {
            perror("picalc: fopen");
            mpz_clear(pi_scaled);
            return 1;
        }
    }

    int result = write_pi(stream, pi_scaled, digits);
    if (output_path != NULL && fclose(stream) != 0) {
        perror("picalc: fclose");
        result = -1;
    }

    mpz_clear(pi_scaled);

    if (result != 0) {
        fputs("picalc: failed to write output\n", stderr);
        return 1;
    }

    return 0;
}
