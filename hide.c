#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

char *argv0;
#include "arg.h"

#ifdef WIN32
#include <io.h>
#define F_OK 0
#define access _access
#else
#include <unistd.h>
#endif

#define INVERT_SIZE 20
#define FILE_EXISTS(x) (!access(x, F_OK))
#define FILE_READABLE(x) (FILE_EXISTS(x) && !access(x, R_OK))
#define FREE(x)        \
	if (x != NULL) \
	free(x)

const uint8_t magick[] = { 0x3, 0xb, 0xa, 0x1, 0xb, 0x1, 0xa };

typedef struct settings {
	char *input; // path to image/video
	char *input_secret; // path to file to hide
	char *output_file; // path to output file
	char mode; // 'c' - create, 'e' - extract
	bool force; // overwrite output file, if already exists
} settings;

void die(const char *errstr, ...)
{
	va_list ap;

	va_start(ap, errstr);
	vfprintf(stderr, errstr, ap);
	va_end(ap);
	exit(1);
}

void usage(int exit_code)
{
	// clang-format off
	fprintf(stderr, "Usage: ");
	fprintf(stderr, "%s [-c | -e] [-y] -i <input file> -s <input secret> -o <output file>\n", argv0);
	fprintf(stderr, "Arguments:\n");
	fprintf(stderr, "\t-i <input file>    - path to input file\n");
	fprintf(stderr, "\t-s <input secret>  - path to file to hide (for create mod only)\n");
	fprintf(stderr, "\t-o <output file>   - path of output file\n");
	fprintf(stderr, "\t-c                 - create mode\n");
	fprintf(stderr, "\t-e                 - exctract mode (default)\n");
	fprintf(stderr, "\t-y                 - force rewrite existing files\n");
	// clang-format on
	exit(exit_code);
	return;
}

void n_usage(void)
{
	usage(0);
	return;
}

uint8_t *read_whole_file(FILE *file, size_t *size)
{
	if (fseek(file, 0, SEEK_END) != 0)
		die("Failed to seek file");
	*size = ftell(file);
	if (fseek(file, 0, SEEK_SET != 0))
		die("Failed to seek file");
	uint8_t *buf = (uint8_t *)calloc(*size, sizeof(uint8_t));
	if (!buf)
		die("Allocation failed\n");
	size_t ret_code = fread(buf, sizeof(uint8_t), *size, file);
	if (ret_code != *size) {
		if (feof(file)) {
			die("Error reading input file: unexpected EOF\n");
		} else if (ferror(file)) {
			die("Error reading input file");
		}
	}
	return buf;
}

void hide_extract(FILE *input, char *output_path)
{
	size_t in_s;
	uint8_t *in = read_whole_file(input, &in_s);
	uint8_t *in_b = in;
	while (in - in_b <= in_s) {
		if (in[0] == magick[0]) {
			for (size_t i = 0; i < sizeof(magick); i++) {
				if (in[i] != magick[i]) {
					goto hide_extract__while_next_iter;
				}
			}
			goto hide_extract__loop_end;
		}
hide_extract__while_next_iter:
		in++;
	}
hide_extract__loop_end:
	for (size_t i = 0; i < sizeof(magick); i++) {
		if (in[i] != magick[i]) {
			die("No magick found in input file\n");
		}
	}
	in = in + sizeof(magick);
	for (size_t i = 0; i < INVERT_SIZE; i++)
		in[i] = ~in[i];

	FILE *out = fopen(output_path, "wb");
	if (!out)
		die("Failed to open output file\n");
	size_t ret_code;
	in_s = in_s - (in - in_b);
	ret_code = fwrite(in, sizeof(uint8_t), in_s, out);
	fclose(out);
	FREE(in_b);
	if (ret_code != in_s)
		die("Failed to write output file\n");
	return;
}

void hide_create(FILE *input, FILE *hide, char *output_path)
{
	size_t in_s, hi_s;
	uint8_t *in = read_whole_file(input, &in_s);
	uint8_t *hi = read_whole_file(hide, &hi_s);
	if (hi_s < INVERT_SIZE)
		die("Input file is too small\n");
	for (size_t i = 0; i < INVERT_SIZE; i++)
		hi[i] = ~hi[i];
	FILE *out = fopen(output_path, "wb");
	if (!out)
		die("Failed to open output file\n");
	size_t ret_code;
	ret_code = fwrite(in, sizeof(uint8_t), in_s, out);
	if (ret_code != in_s)
		goto hide_create__out_write_error;
	ret_code = fwrite(magick, sizeof(uint8_t), sizeof(magick), out);
	if (ret_code != sizeof(magick))
		goto hide_create__out_write_error;
	ret_code = fwrite(hi, sizeof(uint8_t), hi_s, out);
	if (ret_code != hi_s)
		goto hide_create__out_write_error;

	FREE(in);
	FREE(hi);
	fclose(out);

	return;

hide_create__out_write_error:
	die("Failed to write output file\n");
}

int main(int argc, char **argv)
{
	settings opts;

	opts.input = NULL;
	opts.input_secret = NULL;
	opts.output_file = NULL;
	opts.mode = 'e';
	opts.force = false;

	ARGBEGIN
	{
	case 'c':
		opts.mode = 'c';
		break;
	case 'e':
		opts.mode = 'e';
		break;
	case 'i':
		opts.input = EARGF(usage(1));
		break;
	case 's':
		opts.input_secret = EARGF(usage(1));
		break;
	case 'o':
		opts.output_file = EARGF(usage(1));
		break;
	case 'y':
		opts.force = true;
		break;
	case 'h':
		n_usage();
		break;
	}
	ARGEND;

	if (opts.input == NULL ||
	    (opts.input_secret == NULL && opts.mode == 'c') ||
	    opts.output_file == NULL) {
		die("Not enough arguments. See -h\n");
	}

	if (!FILE_READABLE(opts.input)) {
		die("Input file must exist and be readable\n", argv0);
	}
	if (opts.mode == 'c' && !FILE_READABLE(opts.input_secret)) {
		die("Input files must exist and be readable\n", argv0);
	}

	if (FILE_EXISTS(opts.output_file) && !opts.force)
		die("Output file already exists, force flag not set");

	FILE *input = fopen(opts.input, "rb");
	if (!input) {
		die("Fail to open input file %s\n", opts.input);
	}

	if (opts.mode == 'e') {
		hide_extract(input, opts.output_file);
	} else {
		FILE *input_secret = fopen(opts.input_secret, "rb");
		if (!input_secret) {
			die("Fail to open input file %s\n", opts.input);
		}
		hide_create(input, input_secret, opts.output_file);
		fclose(input_secret);
	}
	fclose(input);

	return 0;
}
