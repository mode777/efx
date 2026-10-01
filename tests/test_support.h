#ifndef EFX_TEST_SUPPORT_H
#define EFX_TEST_SUPPORT_H

/*
 * Shared unit-test vocabulary: `fail`, `feq` and the case-table runner.
 * Each suite lists its cases as `EFX_CASE(fn)` entries; tests/CMakeLists.txt
 * reads those tokens from the suite source to register one ctest per case.
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#ifndef EFX_TEST_FEQ_EPS
#define EFX_TEST_FEQ_EPS 0.001f
#endif

typedef struct {
    const char *name;
    int (*fn)(void);
} efx_test_case;

#define EFX_CASE(fn) {#fn, fn}

static inline int fail(const char *what) {
    fprintf(stderr, "FAIL: %s\n", what);
    return 1;
}

static inline int feq(float a, float b) {
    return fabsf(a - b) < EFX_TEST_FEQ_EPS;
}

/* argv[1] runs that case; no argument runs every case; unknown exits 2 */
static inline int efx_test_main(const efx_test_case *cases, size_t n,
                                int argc, char **argv) {
    if (argc > 1) {
        for (size_t i = 0; i < n; i++) {
            if (strcmp(cases[i].name, argv[1]) == 0) return cases[i].fn();
        }
        fprintf(stderr, "unknown case: %s\n", argv[1]);
        return 2;
    }
    int failures = 0;
    for (size_t i = 0; i < n; i++) {
        if (cases[i].fn() != 0) {
            fprintf(stderr, "case failed: %s\n", cases[i].name);
            failures++;
        }
    }
    printf("%zu cases, %d failures\n", n, failures);
    return failures ? 1 : 0;
}

#endif /* EFX_TEST_SUPPORT_H */
