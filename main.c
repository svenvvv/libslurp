#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "slurp.h"

#define BG_COLOR 0xFFFFFF40
#define BORDER_COLOR 0x000000FF
#define SELECTION_COLOR 0x00000000
#define FONT_FAMILY "sans-serif"

static const char usage[] =
	"Usage: slurp [options...]\n"
	"\n"
	"  -h           Show help message and quit.\n"
	"  -d           Display dimensions of selection.\n"
	"  -b #rrggbbaa Set background color.\n"
	"  -c #rrggbbaa Set border color.\n"
	"  -s #rrggbbaa Set selection color.\n"
	"  -B #rrggbbaa Set option box color.\n"
	"  -F s         Set the font family for the dimensions.\n"
	"  -w n         Set border weight.\n"
	"  -f s         Set output format.\n"
	"  -o           Select a display output.\n"
	"  -p           Select a single point.\n"
	"  -r           Restrict selection to predefined boxes.\n"
	"  -a w:h       Force aspect ratio.\n";

static int min(int a, int b) {
	return (a < b) ? a : b;
}

static uint32_t parse_color(const char *color) {
	if (color[0] == '#') {
		++color;
	}

	int len = strlen(color);
	if (len != 6 && len != 8) {
		fprintf(stderr, "Invalid color %s, "
				"defaulting to color 0xFFFFFFFF\n", color);
		return 0xFFFFFFFF;
	}
	uint32_t res = (uint32_t)strtoul(color, NULL, 16);
	if (strlen(color) == 6) {
		res = (res << 8) | 0xFF;
	}
	return res;
}

static void print_output_name(FILE *stream, const struct slurp_box *result, struct wl_list *outputs) {
	struct slurp_output *output = slurp_output_from_box(result, outputs);
	if (output) {
		struct slurp_box *geometry = &output->logical_geometry;
		if (geometry->label) {
			fprintf(stream, "%s", geometry->label);
			return;
		}
	}
	fprintf(stream, "<unknown>");
}

static void print_formatted_result(FILE *stream, struct slurp_state *state , const char *format) {
	struct slurp_output *output = slurp_output_from_box(&state->result, &state->outputs);
	for (size_t i = 0; format[i] != '\0'; i++) {
		char c = format[i];
		if (c == '%') {
			char next = format[i + 1];

			i++; // Skip the next character (x, y, w or h)
			switch (next) {
			case 'x':
				fprintf(stream, "%d", state->result.x);
				continue;
			case 'y':
				fprintf(stream, "%d", state->result.y);
				continue;
			case 'w':
				fprintf(stream, "%d", state->result.width);
				continue;
			case 'h':
				fprintf(stream, "%d", state->result.height);
				continue;
			case 'X':
				assert(output);
				fprintf(stream, "%d", state->result.x - output->logical_geometry.x);
				continue;
			case 'Y':
				assert(output);
				fprintf(stream, "%d", state->result.y - output->logical_geometry.y);
				continue;
			case 'W':
				assert(output);
				fprintf(stream, "%d", min(state->result.width, output->logical_geometry.x + output->logical_geometry.width - state->result.x));
				continue;
			case 'H':
				assert(output);
				fprintf(stream, "%d", min(state->result.height, output->logical_geometry.y + output->logical_geometry.height - state->result.y));
				continue;
			case 'l':
				if (state->result.label) {
					fprintf(stream, "%s", state->result.label);
				}
				continue;
			case 'o':
				print_output_name(stream, &state->result, &state->outputs);
				continue;
			default:
				// If no case was executed, revert i back - we don't need to
				// skip the next character.
				i--;
			}
		}
		fprintf(stream, "%c", c);
	}
}

int main(int argc, char *argv[]) {
	int status = EXIT_SUCCESS;

	char *result_str = 0;
	size_t length;
	FILE *stream = open_memstream(&result_str, &length);

	struct slurp_state state = {
		.colors = {
			.background = BG_COLOR,
			.border = BORDER_COLOR,
			.selection = SELECTION_COLOR,
			.choice = BG_COLOR,
		},
		.border_weight = 2,
		.display_dimensions = false,
		.restrict_selection = false,
		.fixed_aspect_ratio = false,
		.aspect_ratio = 0,
		.font_family = FONT_FAMILY,
		.cursor_size = 24,
		.output_boxes = false,
	};

	int opt;
	char *format = "%x,%y %wx%h\n";
	// bool output_boxes = false;
	int w, h;
	while ((opt = getopt(argc, argv, "hdb:c:s:B:w:proa:f:F:")) != -1) {
		switch (opt) {
		case 'h':
			printf("%s", usage);
			return EXIT_SUCCESS;
		case 'd':
			state.display_dimensions = true;
			break;
		case 'b':
			state.colors.background = parse_color(optarg);
			break;
		case 'c':
			state.colors.border = parse_color(optarg);
			break;
		case 's':
			state.colors.selection = parse_color(optarg);
			break;
		case 'B':
			state.colors.choice = parse_color(optarg);
			break;
		case 'f':
			format = optarg;
			break;
		case 'F':
			state.font_family = optarg;
			break;
		case 'w': {
			errno = 0;
			char *endptr;
			state.border_weight = strtol(optarg, &endptr, 10);
			if (*endptr || errno) {
				fprintf(stderr, "Error: expected numeric argument for -w\n");
				exit(EXIT_FAILURE);
			}
			break;
		}
		case 'p':
			state.single_point = true;
			break;
		case 'o':
			state.output_boxes = true;
			break;
		case 'r':
			state.restrict_selection = true;
			break;
		case 'a':
			if (sscanf(optarg, "%d:%d", &w, &h) != 2) {
				fprintf(stderr, "invalid aspect ratio\n");
				return EXIT_FAILURE;
			}
			if (w <= 0 || h <= 0) {
				fprintf(stderr, "width and height of aspect ratio must be greater than zero\n");
				return EXIT_FAILURE;
			}
			state.fixed_aspect_ratio = true;
			state.aspect_ratio = (double) h / w;
			break;
		default:
			printf("%s", usage);
			return EXIT_FAILURE;
		}
	}

	if (state.single_point && state.restrict_selection) {
		fprintf(stderr, "-p and -r cannot be used together\n");
		return EXIT_FAILURE;
	}

	state.cursor_theme = getenv("XCURSOR_THEME");
	const char *cursor_size_str = getenv("XCURSOR_SIZE");
	if (cursor_size_str != NULL) {
		char *end;
		errno = 0;
		state.cursor_size = strtol(cursor_size_str, &end, 10);
		if (errno != 0 || cursor_size_str[0] == '\0' || end[0] != '\0') {
			fprintf(stderr, "invalid XCURSOR_SIZE value\n");
			return EXIT_FAILURE;
		}
	}

	slurp_state_init(&state);

	if (!isatty(STDIN_FILENO) && !state.single_point) {
		char *line = NULL;
		size_t line_size = 0;
		while (getline(&line, &line_size, stdin) >= 0) {
			struct slurp_box in_box = {0};
			if (sscanf(line, "%d,%d %dx%d %m[^\n]", &in_box.x, &in_box.y,
					&in_box.width, &in_box.height, &in_box.label) < 4) {
				fprintf(stderr, "invalid box format: %s\n", line);
				return EXIT_FAILURE;
			}
			slurp_add_choice_box(&state, &in_box);
			free(in_box.label);
		}
		free(line);
	}

	status = slurp_select(&state);
	if (status != EXIT_SUCCESS) {
		if (state.error != NULL) {
			fprintf(stderr, "%s\n", state.error);
		} else {
			fprintf(stderr, "selection failed due to an unknown reason\n");
		}
		return status;
	}

	if (state.result.width == 0 && state.result.height == 0) {
		fprintf(stderr, "selection cancelled\n");
		status = EXIT_FAILURE;
	} else {
		print_formatted_result(stream, &state, format);
		fclose(stream);
	}

	if (result_str) {
		printf("%s", result_str);
		free(result_str);
	}

	slurp_destroy(&state);

	return status;
}
