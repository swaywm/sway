#include <stdbool.h>
#include "tests.h"

void *__real_malloc(size_t size);
void __real_free(void *ptr);
void *__real_calloc(size_t nmemb, size_t size);
void *__real_realloc(void *ptr, size_t size);

enum wrapper_behavior _memory_behavior = WRAPPER_INVOKE_REAL;

int reset_mem_wrappers(void **state) {
	_memory_behavior = WRAPPER_INVOKE_REAL;
	return 0;
}

void memory_behavior(enum wrapper_behavior behavior) {
	_memory_behavior = behavior;
}

void *__wrap_malloc(size_t size) {
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
