#define _XOPEN_SOURCE // for crypt
#include <pwd.h>
#include <shadow.h>
#include <stdbool.h>
#include <sys/types.h>
#include <unistd.h>
#include <wlr/util/log.h>
#include "swaylock/swaylock.h"
#ifdef __GLIBC__
// GNU, you damn slimy bastard
#include <crypt.h>
#endif

static int comm[2][2];

static void clear_buffer(void *buf, size_t bytes) {
	volatile char *buffer = buf;
	volatile char zero = '\0';
	for (size_t i = 0; i < bytes; ++i) {
		buffer[i] = zero;
	}
}

void run_child(void) {
	/* This code runs as root */
	struct passwd *pwent = getpwuid(getuid());
	if (!pwent) {
		wlr_log_errno(WLR_ERROR, "failed to getpwuid");
		exit(EXIT_FAILURE);
	}
	char *encpw = pwent->pw_passwd;
	if (strcmp(encpw, "x") == 0) {
		struct spwd *swent = getspnam(pwent->pw_name);
		if (!swent) {
			wlr_log_errno(WLR_ERROR, "failed to getspnam");
			exit(EXIT_FAILURE);
		}
		encpw = swent->sp_pwdp;
	}

	/* We don't need any additional logging here because the parent process will
	 * also fail here and will handle logging for us. */
	if (setgid(getgid()) != 0) {
		exit(EXIT_FAILURE);
	}
	if (setuid(getuid()) != 0) {
		exit(EXIT_FAILURE);
	}

	/* This code does not run as root */
	wlr_log(WLR_DEBUG, "prepared to authorize user %s", pwent->pw_name);

	size_t size;
	char *buf;
	while (1) {
		ssize_t amt;
		amt = read(comm[0][0], &size, sizeof(size));
		if (amt == 0) {
			break;
		} else if (amt < 0) {
			wlr_log_errno(WLR_ERROR, "read pw request");
		}
		wlr_log(WLR_DEBUG, "received pw check request");
		buf = malloc(size);
		if (!buf) {
			wlr_log_errno(WLR_ERROR, "failed to malloc pw buffer");
			exit(EXIT_FAILURE);
		}
		size_t offs = 0;
		do {
			amt = read(comm[0][0], &buf[offs], size - offs);
			if (amt <= 0) {
				wlr_log_errno(WLR_ERROR, "failed to read pw");
				exit(EXIT_FAILURE);
			}
			offs += (size_t)amt;
		} while (offs < size);
		bool result = false;
		char *c = crypt(buf, encpw);
		if (c == NULL) {
			wlr_log_errno(WLR_ERROR, "crypt");
		}
		result = strcmp(c, encpw) == 0;
		if (write(comm[1][1], &result, sizeof(result)) != sizeof(result)) {
			wlr_log_errno(WLR_ERROR, "failed to write pw check result");
			clear_buffer(buf, size);
			exit(EXIT_FAILURE);
		}
		clear_buffer(buf, size);
		free(buf);
	}

	clear_buffer(encpw, strlen(encpw));
	exit(EXIT_SUCCESS);
}

void initialize_pw_backend(void) {
	if (geteuid() != 0) {
		wlr_log(WLR_ERROR, "swaylock needs to be setuid to read /etc/shadow");
		exit(EXIT_FAILURE);
	}
	if (pipe(comm[0]) != 0) {
		wlr_log_errno(WLR_ERROR, "failed to create pipe");
		exit(EXIT_FAILURE);
	}
	if (pipe(comm[1]) != 0) {
		wlr_log_errno(WLR_ERROR, "failed to create pipe");
		exit(EXIT_FAILURE);
	}
	pid_t child = fork();
	if (child == 0) {
		close(comm[0][1]);
		close(comm[1][0]);
		run_child();
	} else if (child < 0) {
		wlr_log_errno(WLR_ERROR, "failed to fork");
		exit(EXIT_FAILURE);
	}
	close(comm[0][0]);
	close(comm[1][1]);
	if (setgid(getgid()) != 0) {
		wlr_log_errno(WLR_ERROR, "Unable to drop root");
		exit(EXIT_FAILURE);
	}
	if (setuid(getuid()) != 0) {
		wlr_log_errno(WLR_ERROR, "Unable to drop root");
		exit(EXIT_FAILURE);
	}
}

bool attempt_password(struct swaylock_password *pw) {
	bool result = false;
	size_t len = pw->len + 1;
	size_t offs = 0;
	if (write(comm[0][1], &len, sizeof(len)) < 0) {
		wlr_log_errno(WLR_ERROR, "Failed to request pw check");
		goto ret;
	}
	do {
		ssize_t amt = write(comm[0][1], &pw->buffer[offs], len - offs);
		if (amt < 0) {
			wlr_log_errno(WLR_ERROR, "Failed to write pw buffer");
			goto ret;
		}
		offs += amt;
	} while (offs < len);
	if (read(comm[1][0], &result, sizeof(result)) != sizeof(result)) {
		wlr_log_errno(WLR_ERROR, "Failed to read pw result");
		goto ret;
	}
	wlr_log(WLR_DEBUG, "pw result: %d", result);
ret:
	clear_password_buffer(pw);
	return result;
}
