#include <stdbool.h>
#include "tests.h"

void *__real_malloc(size_t size);
void __real_free(void *ptr);
void *__real_calloc(size_t nmemb, size_t size);
void *__real_realloc(void *ptr, size_t size);

enum wrapper_behavior _memory_behavior = WRAPPER_INVOKE_CMOCKA;
int malloc_callcount = 0,
	free_callcount = 0,
	calloc_callcount = 0,
	realloc_callcount = 0;

int reset_mem_wrappers(void **state) {
	_memory_behavior = WRAPPER_INVOKE_CMOCKA;
	malloc_callcount =
		free_callcount =
		calloc_callcount =
		realloc_callcount = 0;
	return 0;
}

void memory_behavior(enum wrapper_behavior behavior) {
	_memory_behavior = behavior;
}

int malloc_calls() {
	return malloc_callcount;
}

int free_calls() {
	return free_callcount;
}

int calloc_calls() {
	return calloc_callcount;
}

int realloc_calls() {
	return realloc_callcount;
}

int alloc_calls() {
	return malloc_callcount + calloc_callcount;
}

void *__wrap_malloc(size_t size) {
	++malloc_callcount;
	switch (_memory_behavior) {
	case WRAPPER_INVOKE_CMOCKA:
		return test_malloc(size);
	case WRAPPER_DO_ASSERTIONS:
		check_expected(size);
		return mock_type(void *);
	case WRAPPER_INVOKE_REAL:
	default:
		return __real_malloc(size);
	}
}

void __wrap_free(void *ptr) {
	++free_callcount;
	switch (_memory_behavior) {
	case WRAPPER_INVOKE_CMOCKA:
		test_free(ptr);
		break;
	case WRAPPER_DO_ASSERTIONS:
		check_expected_ptr(ptr);
		break;
	case WRAPPER_INVOKE_REAL:
	default:
		__real_free(ptr);
		break;
	}
}

void *__wrap_calloc(size_t nmemb, size_t size) {
	++calloc_callcount;
	switch (_memory_behavior) {
	case WRAPPER_INVOKE_CMOCKA:
		return test_calloc(nmemb, size);
	case WRAPPER_DO_ASSERTIONS:
		check_expected(nmemb);
		check_expected(size);
		return mock_type(void *);
	case WRAPPER_INVOKE_REAL:
	default:
		return __real_calloc(nmemb, size);
	}
}

void *__wrap_realloc(void *ptr, size_t size) {
	++realloc_callcount;
	switch (_memory_behavior) {
	case WRAPPER_INVOKE_CMOCKA:
		return test_realloc(ptr, size);
	case WRAPPER_DO_ASSERTIONS:
		check_expected_ptr(ptr);
		check_expected(size);
		return mock_type(void *);
	case WRAPPER_INVOKE_REAL:
	default:
		return __real_realloc(ptr, size);
	}
}
