#include <wayland-client.h>
#include <cairo/cairo.h>
#include <pango/pangocairo.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include "client/buffer.h"
#include "list.h"
#include "log.h"

static int create_pool_file(size_t size, char **name) {
	static const char template[] = "/sway-client-XXXXXX";
	const char *path = getenv("XDG_RUNTIME_DIR");
		if (!path) {
		return -1;
	}

	int ts = (path[strlen(path) - 1] == '/');

	*name = malloc(
		strlen(template) +
		strlen(path) +
		(ts ? 1 : 0) + 1);
	sprintf(*name, "%s%s%s", path, ts ? "" : "/", template);

	int fd = mkstemp(*name);

	if (fd < 0) {
		return -1;
	}

	if (ftruncate(fd, size) < 0) {
		close(fd);
		return -1;
	}

	return fd;
}

static void buffer_release(void *data, struct wl_buffer *wl_buffer) {
	struct buffer *buffer = data;
	buffer->busy = false;
}

static const struct wl_buffer_listener buffer_listener = {
	.release = buffer_release
};

static struct buffer *create_buffer(struct window *window, struct buffer *buf,
		int32_t width, int32_t height, uint32_t format) {

	uint32_t stride = width * 4;
	uint32_t size = stride * height;

	char *name;
	int fd = create_pool_file(size, &name);
	if (fd == -1) {
		sway_abort("Unable to allocate buffer");
		return NULL; // never reached
	}
	void *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	struct wl_shm_pool *pool = wl_shm_create_pool(window->registry->shm, fd, size);
	buf->buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride, format);
	wl_shm_pool_destroy(pool);
	close(fd);
	unlink(name);
	free(name);
	fd = -1;

	buf->width = width;
	buf->height = height;
	buf->surface = cairo_image_surface_create_for_data(data, CAIRO_FORMAT_ARGB32, width, height, stride);
	buf->cairo = cairo_create(buf->surface);
	buf->pango = pango_cairo_create_context(buf->cairo);

	wl_buffer_add_listener(buf->buffer, &buffer_listener, buf);
	return buf;
}

static void destroy_buffer(struct buffer *buffer) {
	if (buffer->buffer) {
		wl_buffer_destroy(buffer->buffer);
	}
	if (buffer->cairo) {
		cairo_destroy(buffer->cairo);
	}
	if (buffer->surface) {
		cairo_surface_destroy(buffer->surface);
	}
	memset(buffer, 0, sizeof(struct buffer));
}

struct buffer *get_next_buffer(struct window *window) {
	struct buffer *buffer = NULL;

	int i;
	for (i = 0; i < 2; ++i) {
		if (window->buffers[i].busy) {
			continue;
		}
		buffer = &window->buffers[i];
	}

	if (!buffer) {
		return NULL;
	}

	if (buffer->width != window->width || buffer->height != window->height) {
		destroy_buffer(buffer);
	}

	if (!buffer->buffer) {
		if (!create_buffer(window, buffer, window->width, window->height, WL_SHM_FORMAT_ARGB8888)) {
			return NULL;
		}
	}

	window->cairo = buffer->cairo;
	window->buffer = buffer;
	return buffer;
}
