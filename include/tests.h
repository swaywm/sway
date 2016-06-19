#ifndef __TESTS_H
#define __TESTS_H

#include <stddef.h>
#include <setjmp.h>
#include <stdarg.h>
#include <cmocka.h>

enum wrapper_behavior {
    WRAPPER_INVOKE_REAL,
    WRAPPER_INVOKE_CMOCKA,
    WRAPPER_DO_ASSERTIONS,
};

int reset_mem_wrappers(void **state);
void memory_behavior(enum wrapper_behavior behavior);
int malloc_calls();
int free_calls();
int calloc_calls();
int realloc_calls();
int alloc_calls();

#endif
