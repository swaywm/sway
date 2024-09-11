#ifndef _SWAY_ENV_H
#define _SWAY_ENV_H

void env_free(char **envp);

char **env_get_envp();

char **env_setenv(char **envp, char *name, char *value);

#endif
