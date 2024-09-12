#ifndef _SWAY_ENV_H
#define _SWAY_ENV_H

/**
 * Deallocates an environment array created by
 * sway_env_get_envp or sway_env_setenv.
 */
void env_destroy(char **envp);

/**
 * Gets a newly-allocated environment array pointer
 * from the global environment.
 */
char **env_create();

/**
 * Sets or overwrites an environment variable in the given environment.
 * Setting a new variable will reallocate the entire array.
 */
char **env_setenv(char **envp, char *name, char *value);

#endif
