#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <string.h>

extern char **environ;

int _env_var_name_eq(char *env_name, char *cmp) {
	size_t i = 0;
	int reached_eq;
	while (1) {
		char current_char = env_name[i];
		char cmp_char = cmp[i];

		int is_eq = current_char == '=';
		int is_null = current_char == '\0';

		if (is_eq) reached_eq = 1;
		if (is_null && !reached_eq) return 0;
		if (is_eq && cmp_char == '\0') return 1;
		if (is_eq || cmp_char == '\0') return 0;
		if (current_char != cmp_char) return 0;

		i++;
	}
}

typedef struct {
	char *ptr;
	size_t idx;
} _env_info;

_env_info _env_get(char **envp, char *name) {
	char *strp;
	size_t i = 0;
	while ((strp = envp[i]) != NULL) {
		if (_env_var_name_eq(strp, name)) return (_env_info){strp, i};
		i++;
	}

	return (_env_info){NULL, 0};
}

size_t _env_len(char **envp) {
	char *strp;
	size_t i = 0;

	while ((strp = envp[i]) != NULL) {
		i++;
	}

	return i;
}

char **_env_clone(char **envp, size_t reserve) {
	char **new_envp = calloc(_env_len(envp) + 1 + reserve, sizeof(char *));

	char *strp;
	size_t i = 0;

	while ((strp = envp[i]) != NULL) {
		size_t n = strlen(strp) + 1;
		char *new_strp = malloc(n);
		memcpy(new_strp, strp, n);
		new_envp[i] = new_strp;
		i++;
	}

	return new_envp;
}

void env_free(char **envp) {
	char *strp;
	size_t i = 0;
	while ((strp = envp[i]) != NULL) {
		free(strp);
		i++;
	}

	free(envp);
}

// copy the global environment array into a newly-allocated one
// you are responsible for deallocating it after use
char **env_get_envp() { return _env_clone(environ, 0); }

// use env_get_envp() to acquire an envp
// might clone and deallocate the given envp
char **env_setenv(char **envp, char *name, char *value) {
	size_t name_len = strlen(name);
	size_t value_len = strlen(value);
	char *newp = malloc(name_len + value_len + 2);
	memcpy(newp, name, name_len);
	memcpy(newp + name_len + 1, value, value_len);
	newp[name_len] = '=';
	newp[name_len + value_len + 1] = '\0';

	_env_info existing = _env_get(envp, name);
	if (existing.ptr != NULL) {
		free(existing.ptr);
		envp[existing.idx] = newp;
		return envp;
	} else {
		char **new_envp = _env_clone(envp, 1);
		new_envp[_env_len(envp)] = newp;
		env_free(envp);
		return new_envp;
	}
}
