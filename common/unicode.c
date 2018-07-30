#include <stdint.h>
#include <stddef.h>
#include "unicode.h"
#include "util.h"

size_t utf8_chsize(uint32_t ch) {
	if (ch < 0x80) {
		return 1;
	} else if (ch < 0x800) {
		return 2;
	} else if (ch < 0x10000) {
		return 3;
	}
	return 4;
}

static const uint8_t masks[] = {
	0x7F,
	0x1F,
	0x0F,
	0x07,
	0x03,
	0x01
};

uint32_t utf8_decode(const char **char_str) {
	uint8_t **s = (uint8_t **)char_str;

	uint32_t cp = 0;
	if (**s < 128) {
		// shortcut
		cp = **s;
		++*s;
		return cp;
	}
	int size = utf8_size((char *)*s);
	if (size == -1) {
		++*s;
		return UTF8_INVALID;
	}
	uint8_t mask = masks[size - 1];
	cp = **s & mask;
	++*s;
	while (--size) {
		cp <<= 6;
		cp |= **s & 0x3f;
		++*s;
	}
	return cp;
}

size_t utf8_encode(char *str, uint32_t ch) {
	size_t len = 0;
	uint8_t first;

	if (ch < 0x80) {
		first = 0;
		len = 1;
	} else if (ch < 0x800) {
		first = 0xc0;
		len = 2;
	} else if (ch < 0x10000) {
		first = 0xe0;
		len = 3;
	} else {
		first = 0xf0;
		len = 4;
	}

	for (size_t i = len - 1; i > 0; --i) {
		str[i] = (ch & 0x3f) | 0x80;
		ch >>= 6;
	}

	str[0] = ch | first;
	return len;
}


static const struct {
	uint8_t mask;
	uint8_t result;
	int octets;
} sizes[] = {
	{ 0x80, 0x00, 1 },
	{ 0xE0, 0xC0, 2 },
	{ 0xF0, 0xE0, 3 },
	{ 0xF8, 0xF0, 4 },
	{ 0xFC, 0xF8, 5 },
	{ 0xFE, 0xF8, 6 },
	{ 0x80, 0x80, -1 },
};

int utf8_size(const char *s) {
	uint8_t c = (uint8_t)*s;
	for (size_t i = 0; i < ARRAY_SIZE(sizes); ++i) {
		if ((c & sizes[i].mask) == sizes[i].result) {
			return sizes[i].octets;
		}
	}
	return -1;
}
