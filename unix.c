//Copyright (C) 2021 Daniel Bokser.  See LICENSE file for license
#include <sys/mman.h>
#define NL "\n"

//unity build
#include "3rdparty/miniz.c"
#include "aseprite_ssd1306.c"

static ProgramArgs parse_args(int argc, char **argv) {
	ProgramArgs ret = {0};
	if (argc < 2) {
		return ret;
	}
	for (int i = 1; i < argc; i++) {
		char *arg = argv[i];
		if (*arg == '-') {
			switch (arg[1]) {
				case 'p':
					ret.should_show_python = true;
					break;
				case 'v':
					ret.should_show_frames = true;
					break;
				default:
					return ret;
			}
		}
		else {
			ret.in_file_name = arg;
		}
	}

	if (ret.should_show_frames || ret.should_show_python) {
		ret.is_valid = argc == 3;
	} 
	else {
		ret.is_valid = argc == 2;
	}

	return ret;
}

int main(int argc, char **argv) {
    ProgramArgs pa = parse_args(argc, argv);

	if (!pa.is_valid) {
		PRINTERR("Aseprite-SSD1306 Utility " ASEPRITE_SSD1306_VERSION);
		PRINTERR("Usage %s [-pv] aseprite_file", argv[0]);
		return 1;
	}
	const char *in_file_name = pa.in_file_name;
	FILE *f = fopen(in_file_name, "rb");
	if (!f) {
        PRINTERR("Error opening '%s' -- %s",  in_file_name, strerror(errno));
        exit(1);
	}

	fseek(f, 0L, SEEK_END); 
	long file_size = ftell(f);
	if (file_size < sizeof(AsepriteHeader)) {
		PRINTERR("'%s' is not a valid Aseprite file!", in_file_name);
		exit(1);
	}

	rewind(f);
	
	ByteStackAllocator program_allocator = {0};
	usize program_bytes_required = MB(4)+file_size;
	program_allocator.data = program_allocator.cursor = mmap(NULL, program_bytes_required, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	if (!program_allocator.data) {
		PRINTERR("Failed to allocate required %zu bytes!  Exiting...", program_bytes_required);
		exit(1);
	}
	program_allocator.capacity = program_bytes_required;
	u8 *file_buffer = push_bytes(file_size, &program_allocator);
	int read_result = fread(file_buffer, file_size, 1, f);
	if (read_result != 1) {
		PRINTERR("Failed to read in Aseprite file! -- %s", strerror(errno));
        exit(1);
	}
	fclose(f);

    aseprite_to_ssd1306(pa, file_buffer, file_size, program_allocator);

	return 0;
}