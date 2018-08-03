#ifndef _SWAY_SWAYNAG_H
#define _SWAY_SWAYNAG_H

struct swaynag_instance {
	const char *args;
	pid_t pid;
	int fd[2];
	bool detailed;
};

// Copy all fields of one instance to another
void swaynag_clone(struct swaynag_instance *dest,
		struct swaynag_instance *src);

// Spawn swaynag. If swaynag->detailed, then swaynag->fd[1] will left open
// so it can be written to. Call swaynag_show when done writing. This will
// be automatically called by swaynag_log if the instance is not spawned and
// swaynag->detailed is true.
bool swaynag_spawn(const char *swaynag_command,
		struct swaynag_instance *swaynag);

// Kill the swaynag instance
void swaynag_kill(struct swaynag_instance *swaynag);

// Write a log message to swaynag->fd[1]. This will fail when swaynag->detailed
// is false.
void swaynag_log(const char *swaynag_command, struct swaynag_instance *swaynag,
		const char *fmt, ...);

// If swaynag->detailed, close swaynag->fd[1] so swaynag displays
void swaynag_show(struct swaynag_instance *swaynag);

#endif
