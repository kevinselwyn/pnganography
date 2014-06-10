/*
 * PNG image steganography utility
 *
 * Copyright (C) 2014, Kevin Selwyn <kevinselwyn at gmail dot com>
 * All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <png.h>

#if !defined(__APPLE__)
#include <malloc.h>
#endif

#define VERSION "1.0.1"

void error(char *msg, ...) {
	va_list args;
	va_start(args, msg);
	char *data = va_arg(args, char*);

	if (data) {
		printf(msg, data);
	} else {
		printf(msg);
	}

	exit(1);
}

struct png_data {
	int *bits, width, height;
	char *header[8];
	png_byte color_type, bit_depth;
	png_structp png_ptr;
	png_infop info_ptr;
	png_bytep *row_pointers;
};

struct text_data {
	int *bits, content_size, bits_size;
	char *content;
};

void free_png(struct png_data p) {
	int y;

	for (y = 0; y < p.height; y++) {
		free(p.row_pointers[y]);
	}

	free(p.row_pointers);
}

struct png_data read_png(char *filename) {
	FILE *file = NULL;
	struct png_data p;
	int y;

	file = fopen(filename, "rb");

	if (!file) {
		error("Could not open %s\n", filename);
	}

	if (fread(p.header, 1, 8, file) != 8) {
		error("Could not read %s\n", filename);
	}

	if (png_sig_cmp(p.header, 0, 8)) {
		error("%s is not a PNG\n", filename);
	}

	p.png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

	if (!p.png_ptr) {
		error("png_create_read_struct failed\n");
	}

	p.info_ptr = png_create_info_struct(p.png_ptr);

	if (!p.info_ptr) {
		error("png_create_info_struct failed\n");
	}

	if (setjmp(png_jmpbuf(p.png_ptr))) {
		error("Error initializing read\n");
	}

	png_init_io(p.png_ptr, file);
	png_set_sig_bytes(p.png_ptr, 8);
	png_read_info(p.png_ptr, p.info_ptr);

	p.width = png_get_image_width(p.png_ptr, p.info_ptr);
	p.height = png_get_image_height(p.png_ptr, p.info_ptr);
	p.color_type = png_get_color_type(p.png_ptr, p.info_ptr);
	p.bit_depth = png_get_bit_depth(p.png_ptr, p.info_ptr);

	png_read_update_info(p.png_ptr, p.info_ptr);

	if (setjmp(png_jmpbuf(p.png_ptr))) {
		error("Could not read PNG\n");
	}

	p.row_pointers = (png_bytep*) malloc(sizeof(png_bytep) * p.height);

	for (y = 0; y < p.height; y++) {
		p.row_pointers[y] = (png_byte*) malloc(png_get_rowbytes(p.png_ptr, p.info_ptr));
	}

	png_read_image(p.png_ptr, p.row_pointers);

	fclose(file);

	return p;
}

struct png_data write_png(struct png_data p, char *filename) {
	FILE *output = NULL;

	output = fopen(filename, "wb");

	if (!output) {
		error("Could not open %s for writing\n", filename);
	}

	p.png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

	if (!p.png_ptr) {
		error("png_create_write_struct failed\n");
	}

	p.info_ptr = png_create_info_struct(p.png_ptr);

	if (!p.info_ptr) {
		error("png_create_info_struct failed\n");
	}

	if (setjmp(png_jmpbuf(p.png_ptr))) {
		error("Error initializing write\n");
	}

	png_init_io(p.png_ptr, output);

	if (setjmp(png_jmpbuf(p.png_ptr))) {
		error("Error writing header\n");
	}

	png_set_IHDR(p.png_ptr, p.info_ptr, p.width, p.height, p.bit_depth, p.color_type, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
	png_write_info(p.png_ptr, p.info_ptr);

	if (setjmp(png_jmpbuf(p.png_ptr))) {
		error("Error writing bytes\n");
	}

	png_write_image(p.png_ptr, p.row_pointers);

	if (setjmp(png_jmpbuf(p.png_ptr))) {
		error("Error ending write\n");
	}

	png_write_end(p.png_ptr, NULL);

	fclose(output);

	return p;
}

int png_compare(struct png_data p1, struct png_data p2) {
	if (p1.width != p2.width || p1.height != p2.height || p1.color_type != p2.color_type || p1.bit_depth != p2.bit_depth) {
		return 0;
	}

	return 1;
}

int png_color_mode(int color_type) {
	int color_mode;

	switch (color_type) {
	case PNG_COLOR_TYPE_GRAY:
		color_mode = 1;
		break;
	default:
		color_mode = 3;
	}

	return color_mode;
}

struct text_data read_text(char *filename) {
	FILE *text = NULL;
	struct text_data t;
	int byte, i, j, k, l;

	text = fopen(filename, "rb");

	if (!text) {
		error("Cannot open %s\n", filename);
	}

	fseek(text, 0, SEEK_END);
	t.content_size = ftell(text);
	fseek(text, 0, SEEK_SET);

	if (!t.content_size || t.content_size == 0) {
		error("%s is empty\n", filename);
	}

	t.content = malloc((sizeof(char) * t.content_size) + 1);

	if (!t.content) {
		error("Memory error\n");
	}

	if (fread(t.content, 1, t.content_size, text) != t.content_size) {
		error("Unable to read %s\n", filename);
	}

	t.bits = malloc((sizeof(int) * (t.content_size * 8)) + 1);

	if (!t.bits) {
		error("Memory error\n");
	}

	for (i = 0, j = t.content_size, t.bits_size = 0; i < j; i++) {
		byte = t.content[i];

		for (k = 128, l = 1; k >= 1; k /= 2) {
			t.bits[t.bits_size++] = ((byte & k) == k) ? 0 : 1;
		}
	}

	t.bits_size = t.content_size * 8;

	fclose(text);

	return t;
}

struct text_data write_text(struct text_data t, char *filename) {
	FILE *output = NULL;
	int byte, counter, i, j, k;

	output = fopen(filename, "w");

	if (!output) {
		error("Could not open %s for writing\n", filename);
	}

	t.content_size = t.bits_size / 8;
	t.content = malloc((sizeof(char) * t.content_size) + 1);

	if (!t.content) {
		error("Memory error\n");
	}

	for (i = 0, byte = 0, counter = 0; i < t.bits_size; i += 8) {
		byte = 0;

		for (j = 0; j < 8; j++) {
			if (j != 0) {
				byte <<= 1;
			}

			byte |= t.bits[i + j] ^ 1;
		}

		t.content[counter++] = byte;
	}

	if (fwrite(t.content, 1, t.content_size, output) != t.content_size) {
		error("Error writing %s\n", filename);
	}

	fclose(output);
	free(t.content);

	return t;
}

int encode(char *key, char *secret, char *output) {
	struct png_data p = read_png(key);
	struct text_data t = read_text(secret);
	int color_mode, counter, pixels, stop, i, x, y;
	char *plural;

	color_mode = png_color_mode(p.color_type);

	if (t.bits_size > (p.width * p.height * color_mode)) {
		error("Embedded file size is too large\n");
	}

	for (y = 0, counter = 0, pixels = 0, stop = 0; y < p.height; y++) {
		png_byte *row = p.row_pointers[y];

		for (x = 0; x < p.width; x++, pixels++) {
			png_byte *rgb = &(row[x * color_mode]);

			for (i = 0; i < color_mode; i += 1) {
				rgb[i] = (rgb[i] & ~1) | t.bits[counter++];
			}

			if (counter > t.bits_size) {
				stop = 1;
				break;
			}
		}

		if (stop) {
			break;
		}
	}

	t.bits_size -= t.bits_size % 8;

	plural = (counter == 8) ? "" : "s";
	printf("Message successfully hidden\n%d pixel%s affected\n", pixels, plural);

	write_png(p, output);

	free_png(p);

	return 0;
}

int decode(char *key, char *secret, char *output) {
	struct png_data p1 = read_png(key);
	struct png_data p2 = read_png(secret);
	struct text_data t;
	int color_mode, pixels, counter, stop, test, i, x, y;
	char *plural;

	color_mode = png_color_mode(p1.color_type);

	if (!png_compare(p1, p2)) {
		error("PNG headers do not match\n");
	}

	for (y = p1.height - 1, pixels = p1.width * p1.height, stop = 0; y >= 0; y--) {
		png_byte *row1 = p1.row_pointers[y];
		png_byte *row2 = p2.row_pointers[y];

		for (x = p1.width - 1; x >= 0; x--, pixels--) {
			png_byte *rgb1 = &(row1[x * color_mode]);
			png_byte *rgb2 = &(row2[x * color_mode]);

			for (i = 0, test = 0; i < color_mode; i++) {
				if (rgb1[0] != rgb2[0]) {
					test++;
				}
			}

			if (test == color_mode) {
				stop = 1;
				break;
			}
		}

		if (stop) {
			break;
		}
	}

	pixels += 1;
	t.bits = malloc((sizeof(int) * (pixels * color_mode)) + 1);

	for (y = 0, counter = 0, stop = 0; y < p2.height; y++) {
		png_byte *row2 = p2.row_pointers[y];

		for (x = 0; x < p2.width; x++) {
			png_byte *rgb2 = &(row2[x * color_mode]);

			for (i = 0; i < color_mode; i++) {
				t.bits[counter++] = rgb2[i] & 1;
			}

			if (counter == pixels * color_mode) {
				stop = 1;
				break;
			}
		}

		if (stop) {
			break;
		}
	}

	plural = (counter == 8) ? "" : "s";
	printf("Message successfully recovered\n%d byte%s found\n", counter / 8, plural);

	t.bits_size = pixels * color_mode;
	t.bits_size -= (t.bits_size % 8) + 1;
	t.bits_size = (round(t.bits_size / 8) * 8) + 8;

	write_text(t, output);

	free_png(p1);
	free_png(p2);
	free(t.bits);

	return 0;
}

int main(int argc, char *argv[]) {
	int rc = 0;

	if (argc < 5) {
		error("pnganography (v%s)\n\nUsage: pnganography encode|decode <key.png> <secret> <output>\n", VERSION);
	}

	char *action = argv[1];

	if (strcmp(action, "encode") != 0 && strcmp(action, "decode") != 0) {
		error("Invalid action: %s\n", action);
	}

	char *key = argv[2];
	char *secret = argv[3];
	char *output = argv[4];

	if (strcmp(action, "encode") == 0) {
		rc = encode(key, secret, output);
	} else {
		rc = decode(key, secret, output);
	}

	return rc;
}
