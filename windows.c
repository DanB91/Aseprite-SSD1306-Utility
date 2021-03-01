//Copyright (C) 2021 Daniel Bokser.  See LICENSE file for license
#include <windows.h>
#define NL "\r\n"

//unity build
#include "3rdparty/miniz.c"
#include "aseprite_ssd1306.c"

#define PRINTERRNO_EXIT(err_no, fmt, ...) do {\
	char err_buf[128] = {0};\
	strerror_s(err_buf, sizeof(err_buf), err_no);\
	PRINTERR(fmt" -- %s", ##__VA_ARGS__, err_buf);\
	exit(1);\
	}while(0)

static ProgramArgs parse_args(int argc, wchar_t **argv) {
	ProgramArgs ret = {0};
	if (argc < 2) {
		return ret;
	}
	for (int i = 1; i < argc; i++) {
		wchar_t *arg = argv[i];
		if (*arg == L'-') {
			switch (arg[1]) {
				case L'p':
					ret.should_show_python = true;
					break;
				case L'v':
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

int wmain(int argc, wchar_t **argv) {
    ProgramArgs pa = parse_args(argc, argv);

	if (!pa.is_valid) {
		PRINTERR("Aseprite-SSD1306 Utility " ASEPRITE_SSD1306_VERSION);
		PRINTERR("Usage %ls [-pv] aseprite_file", argv[0]);
		return 1;
	}
	const wchar_t *in_file_name = pa.in_file_name;
	FILE *f;
	if (_wfopen_s(&f, in_file_name, L"rb") != 0) {
		PRINTERRNO_EXIT(errno, "Error opening '%ls'", in_file_name);
	}

	fseek(f, 0L, SEEK_END); 
	long file_size = ftell(f);
	if (file_size < sizeof(AsepriteHeader)) {
		PRINTERR("'%ls' is not a valid Aseprite file!", in_file_name);
		exit(1);
	}

	rewind(f);
	
	ByteStackAllocator program_allocator = {0};
	usize program_bytes_required = MB(4)+file_size;
	program_allocator.data = program_allocator.cursor = VirtualAlloc(NULL, program_bytes_required, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!program_allocator.data) {
		PRINTERR("Failed to allocate required %zu bytes!  Exiting...", program_bytes_required);
		exit(1);
	}
	program_allocator.capacity = program_bytes_required;
	u8 *file_buffer = push_bytes(file_size, &program_allocator);
	int read_result = fread(file_buffer, file_size, 1, f);
	if (read_result != 1) {
		PRINTERRNO_EXIT(errno, "Failed to read in Aseprite file!");
	}
	fclose(f);

    aseprite_to_ssd1306(pa, file_buffer, file_size, program_allocator);

	return 0;
}