//Copyright (C) 2021 Daniel Bokser.  See LICENSE file for license

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>

#include <stdbool.h>

//unity build
#include "3rdparty/miniz.c"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef size_t usize;
typedef ptrdiff_t isize;

#define GB(n) (n*MB(1024))
#define MB(n) (n*KB(1024))
#define KB(n) (n*1024)
#define PRINTLN(fmt, ...) printf(fmt NL, ##__VA_ARGS__)
#define PRINTERR(fmt, ...) fprintf(stderr, fmt NL, ##__VA_ARGS__)

#if RELEASE
//#   define NDEBUG
#	define DEBUGOUT(fmt, ...)
#	define DEBUGOUTLN(fmt, ...)
#else
#	define DEBUGOUT(fmt, ...) printf(fmt, ##__VA_ARGS__)
#	define DEBUGOUTLN(fmt, ...) printf(fmt NL, ##__VA_ARGS__)
#endif
#include <assert.h>

//Cel Chunk Types (CCT)
#define CCT_RAW_CEL 0
#define CCT_LINKED_CEL 1
#define CCT_COMPRESSED_CEL 2

typedef struct AsepriteLayerChunkHeader {
	/*
	 *
	 1 = Visible
	 2 = Editable
	 4 = Lock movement
	 8 = Background
	 16 = Prefer linked cels
	 32 = The layer group should be displayed collapsed
	 64 = The layer is a reference layer
	 */
	u16 flags;
	/*
	 * 
	 0 = Normal (image) layer
	 1 = Group
	 */
	u16 layer_type; 
	u16 layer_child_level;
	u16 default_layer_width;
	u16 default_layer_height;
	/*
	 *
	 *   Normal         = 0
	 Multiply       = 1
	 Screen         = 2
	 Overlay        = 3
	 Darken         = 4
	 Lighten        = 5
	 Color Dodge    = 6
	 Color Burn     = 7
	 Hard Light     = 8
	 Soft Light     = 9
	 Difference     = 10
	 Exclusion      = 11
	 Hue            = 12
	 Saturation     = 13
	 Color          = 14
	 Luminosity     = 15
	 Addition       = 16
	 Subtract       = 17
	 Divide         = 18
	*/
	u16 blend_mode;

	u8 opacity;
	u8 reserved[3];
	u16 layer_name_len;

}__attribute__((packed)) AsepriteLayerChunkHeader;
typedef struct AsepriteGrayscalePixel {
	u8 value;
	u8 alpha;
}__attribute__((packed)) AsepriteGrayscalePixel;
typedef struct AsepriteRGBAPixel {
	u8 red;
	u8 green;
	u8 blue;
	u8 alpha;
}__attribute__((packed)) AsepriteRGBAPixel;
typedef struct AsepriteCelChunkHeader {
	u16 layer_index;
	i16 x;
	i16 y;
	u8 opacity;
	u16 type;
	u8 unused[7];
} __attribute__((packed)) AsepriteCelChunkHeader;
typedef struct AsepriteRawAndCompressedCelHeader {
	u16 width;
	u16 height;
} __attribute__((packed)) AsepriteRawAndCompressedCelHeader;
typedef struct AsepriteLinkedCelHeader {
	u16 frame_to_link_with;
} __attribute__((packed)) AsepriteLinkedCelHeader;

typedef struct AsepriteChunkHeader {
	u32 size;
	u16 type;
} __attribute__((packed)) AsepriteChunkHeader;

typedef struct AsepriteFrameHeader {
	u32 frame_size;
	u16 magic; //must be 0xF1FA
	u16 old_number_of_chunks; //If this value is 0xFFFF, we might have more chunks to read in this frame
	u16 frame_duration_ms;
	u8 reserved[2];
	u32 number_of_chunks; //if this is 0, use the old field
} __attribute__((packed)) AsepriteFrameHeader;

typedef struct AsepriteHeader {
	u32 file_size;
	u16 magic; //Needs to be 0xA5E0
	u16 frames;
	u16 width;
	u16 height;
	u16 color_depth;
	u32 flags;
	u16 speed; //deprecated
	u64 zero;
	u8 transparent_color_index; //only for indexed sprites
	u8 ignore[3];
	u16 number_of_colors;
	u8 pixel_width;
	u8 pixel_height; //if this or pixel_width is zero, pixel ratio is 1:1
	i16 x_position_of_grid;
	i16 y_position_of_grid;
	u16 grid_width; //zero if no grid
	u16 grid_height; //zero if no grid
	u8 unused[84];

} __attribute__((packed)) AsepriteHeader;


typedef struct ByteStackAllocator {
	u8 *data;
	usize capacity;
	u8 *cursor;
} ByteStackAllocator;

typedef struct ProgramArgs {
	bool should_show_frames;
	bool should_show_python;
	bool is_valid;
#ifdef _WIN32
	const wchar_t *in_file_name;
#else
	const char *in_file_name;
#endif
} ProgramArgs;


void *push_bytes(usize num_bytes, ByteStackAllocator *allocator) {
	//align to 8 byte boundary
	if ((num_bytes % 8) != 0) {
		num_bytes += 8 - (num_bytes % 8);
	}
	assert(allocator->cursor + num_bytes - allocator->data < allocator->capacity);
	u8 *ret = allocator->cursor;
	allocator->cursor += num_bytes;
	return ret;

}

void aseprite_to_ssd1306(ProgramArgs pa, u8 *file_buffer, usize file_size, ByteStackAllocator program_allocator) {
	assert(sizeof(AsepriteHeader) == 128);
	assert(sizeof(AsepriteFrameHeader) == 16);
	assert(sizeof(AsepriteChunkHeader) == 6);
	assert(sizeof(AsepriteLinkedCelHeader) == 2);
	assert(sizeof(AsepriteRawAndCompressedCelHeader) == 4);
	assert(sizeof(AsepriteRGBAPixel) == 4);
	assert(sizeof(AsepriteGrayscalePixel) == 2);
	assert(sizeof(AsepriteLayerChunkHeader) == 18);

	AsepriteHeader *file_header = (AsepriteHeader*)file_buffer;

	//Validation
	if (file_header->magic != 0xA5E0) {
		PRINTERR("Invalid Aseprite file!");
		exit(1);
	}


	if (file_header->color_depth == 8) {
		PRINTERR("Paletted images are not supported yet.");
		exit(1);
	}
	if (file_header->color_depth != 32 && file_header->color_depth != 16) {
		PRINTERR("Invalid color depth in Aseprite file!");
		exit(1);
	}
	//End Validation
	
	u16 byte_height = file_header->height/8;
	if (byte_height == 0) byte_height = 1;	

    if (!pa.should_show_frames) {
        if (pa.should_show_python) {
            PRINTLN("#Image width: %u pixels, or %u bytes, height: %u pixels, or %u bytes", file_header->width, file_header->width, file_header->height, byte_height);
        }
        else {
            PRINTLN("//Image width: %u pixels, or %u bytes, height: %u pixels, or %u bytes", file_header->width, file_header->width, file_header->height, byte_height);
        }
    }
	u8 *output_frames = push_bytes(file_header->width*file_header->height*file_header->frames, &program_allocator);

	file_buffer += sizeof(AsepriteHeader);

	AsepriteFrameHeader *frame_header = (AsepriteFrameHeader*)file_buffer;
	const usize decompression_buffer_len = file_header->width * file_header->height * (file_header->color_depth / 8);
	u8 *decompression_buffer = push_bytes(decompression_buffer_len, &program_allocator);


    struct {
		u8 *data;
		usize len;
		usize capacity;
	} visible_layer_bit_list = {0};
	visible_layer_bit_list.capacity = 2048; //in bits
	visible_layer_bit_list.data = push_bytes((visible_layer_bit_list.capacity/8), &program_allocator);

	//loop through frames
	for (u16 frames_index = 0; 
			file_buffer < file_buffer + file_size && frames_index < file_header->frames; 
			frames_index++,file_buffer += frame_header->frame_size) {
		frame_header = (AsepriteFrameHeader*)file_buffer;
		assert(frame_header->magic == 0xF1FA);

		DEBUGOUTLN("Frame size %u", frame_header->frame_size);
		u8 *frame_data = (u8*)frame_header + sizeof(AsepriteFrameHeader);
		u32 num_chunks = (frame_header->number_of_chunks > 0) ? frame_header->number_of_chunks : frame_header->old_number_of_chunks;
		AsepriteChunkHeader *chunk_header = (AsepriteChunkHeader*)frame_data;
		u8 *frame_bitmap = &output_frames[frames_index*file_header->height*file_header->width];
		//loop through chunks
		for (u32 chunk_index = 0; 
				frame_data < file_buffer + file_size && chunk_index < num_chunks; 
				chunk_index++,frame_data += chunk_header->size) {

			chunk_header = (AsepriteChunkHeader*)frame_data;
			DEBUGOUTLN("Chunk type: 0x%X", chunk_header->type);
			u8 *chunk_data = frame_data + sizeof(AsepriteChunkHeader);
            switch (chunk_header->type) {
            case 0x2004: { //layer chunk
				AsepriteLayerChunkHeader *layer_chunk = (AsepriteLayerChunkHeader*)chunk_data;
				visible_layer_bit_list.len++;
				if (visible_layer_bit_list.len > visible_layer_bit_list.capacity) {
					//TODO: i don't like this.  this assumes the file is well formed.  maybe parse 0x2004 first and then 0x2005.  or just bite the bullet and use realloc (which can't guarentee zero memory)
					//this pushes the cursor forward.  this should be the only caller of push_bytes() so until we get to 0x2005, so we can just assume the list grew
					//and since we have a zero page, we can assume the new data is zero
					push_bytes(visible_layer_bit_list.capacity*2/8, &program_allocator);
					visible_layer_bit_list.capacity *= 2;
				}
				if ((layer_chunk->flags & 1) != 0) {
					visible_layer_bit_list.data[visible_layer_bit_list.len/8] |= 1 << ((visible_layer_bit_list.len-1)%8);
				}
				else {
					DEBUGOUTLN("Not visible!");
				}

			} break;
			
            case 0x2005: { //cel chunk

                AsepriteCelChunkHeader *cel_chunk_header = (AsepriteCelChunkHeader*)chunk_data;
                DEBUGOUTLN("Layer Index: %d", cel_chunk_header->layer_index);
                if (! (visible_layer_bit_list.data[cel_chunk_header->layer_index/8] & (1 << cel_chunk_header->layer_index%8))) {
                    continue;
                }
                switch (cel_chunk_header->type) {
					case CCT_RAW_CEL: {
                        u8 *cel_header_data = chunk_data + sizeof(AsepriteCelChunkHeader);
                        AsepriteRawAndCompressedCelHeader *rac_cel_header = (AsepriteRawAndCompressedCelHeader*)cel_header_data;
					    u8 *data = cel_header_data + sizeof(AsepriteRawAndCompressedCelHeader);
					    if (file_header->color_depth == 32) {
					        AsepriteRGBAPixel *pixels = (AsepriteRGBAPixel*)data;
					        for (int y = 0; y < rac_cel_header->height; y++) {
					      	  for (int x = 0; x < rac_cel_header->width; x++) {
					      		  frame_bitmap[((y+cel_chunk_header->y)*file_header->width) + x + cel_chunk_header->x] = pixels[y*rac_cel_header->width + x].alpha > 0;
					      		  DEBUGOUT("%d", pixels[y*rac_cel_header->width + x].alpha > 0);
					      	  }
					      	  DEBUGOUT("\n");
					        }
					    }
					    else if (file_header->color_depth == 16) {
					        AsepriteGrayscalePixel *pixels = (AsepriteGrayscalePixel*)data;
					        for (int y = 0; y < rac_cel_header->height; y++) {
					      	  for (int x = 0; x < rac_cel_header->width; x++) {
					      		  frame_bitmap[((y+cel_chunk_header->y)*file_header->width) + x + cel_chunk_header->x] = pixels[y*rac_cel_header->width + x].alpha > 0;
					      		  DEBUGOUT("%d", pixels[y*rac_cel_header->width + x].alpha > 0);
					      	  }
					      	  DEBUGOUT("\n");
					        }
					    }
					    else {
					        PRINTERR("Invalid color depth in cel chunk! Either the file is corrupted, or there is a bug in this program (probably the latter).");
					        exit(1);
					    }
					  } break;
                    case CCT_LINKED_CEL:
                        //TODO linked cell
						assert(0);
                        break;
                    case CCT_COMPRESSED_CEL: {
                        u8 *cel_header_data = chunk_data + sizeof(AsepriteCelChunkHeader);
                        AsepriteRawAndCompressedCelHeader *rac_cel_header = (AsepriteRawAndCompressedCelHeader*)cel_header_data;
                        u8 *compressed_data = cel_header_data + sizeof(AsepriteRawAndCompressedCelHeader);
                        mz_ulong tmp_buffer_len = decompression_buffer_len; 
                        int decompression_result = uncompress(decompression_buffer, &tmp_buffer_len, compressed_data, chunk_header->size - sizeof(AsepriteChunkHeader) - sizeof(AsepriteRawAndCompressedCelHeader));
						if (decompression_result != MZ_OK) {
							PRINTERR("Invalid compressed data in cel chunk! Either the file is corrupted, or there is a bug in this program (probably the latter).");
							exit(1);
						}
                        if (file_header->color_depth == 32) {
                            AsepriteRGBAPixel *pixels = (AsepriteRGBAPixel*)decompression_buffer;
                            for (int y = 0; y < rac_cel_header->height; y++) {
                                for (int x = 0; x < rac_cel_header->width; x++) {
									frame_bitmap[((y+cel_chunk_header->y)*file_header->width) + x + cel_chunk_header->x] = pixels[y*rac_cel_header->width + x].alpha > 0;
                                    DEBUGOUT("%d", pixels[y*rac_cel_header->width + x].alpha > 0);
                                }
                                DEBUGOUT("\n");
                            }
                        }
                        else if (file_header->color_depth == 16) {
                            AsepriteGrayscalePixel *pixels = (AsepriteGrayscalePixel*)decompression_buffer;
                            for (int y = 0; y < rac_cel_header->height; y++) {
                                for (int x = 0; x < rac_cel_header->width; x++) {
									frame_bitmap[((y+cel_chunk_header->y)*file_header->width) + x + cel_chunk_header->x] = pixels[y*rac_cel_header->width + x].alpha > 0;
                                    DEBUGOUT("%d", pixels[y*rac_cel_header->width + x].alpha > 0);
                                }
                                DEBUGOUT("\n");
                            }
                        }
                        else {
							PRINTERR("Invalid color depth in cel chunk! Either the file is corrupted, or there is a bug in this program (probably the latter).");
							exit(1);
                        }



                    } break;
                }
                DEBUGOUTLN("Cel Chunk type: 0x%X", cel_chunk_header->type);
            } break;
            }
			
		}
	}


	if (pa.should_show_frames) {
		for (int f = 0; f < file_header->frames; f++) {
			for (int y = 0; y < file_header->height; y++) {
				for (int x = 0; x < file_header->width; x++) {
					printf("%d", output_frames[f*file_header->height*file_header->width + y*file_header->width + x]);
				}
				printf("\n");
			}
			printf("\n\n");
		}
	}
    else if (pa.should_show_python) {
		printf("animation = [\n");
		for (int f = 0; f < file_header->frames; f++) {
			printf("    [\n");
			for (int y = 0; y < file_header->height; y+=8) {
				printf("        [");
				for (int x = 0; x < file_header->width; x++) {
					u8 pixel_data = 0;
					pixel_data |= output_frames[f*file_header->height*file_header->width + (y+0)*file_header->width + x] << 0;
					pixel_data |= output_frames[f*file_header->height*file_header->width + (y+1)*file_header->width + x] << 1;
					pixel_data |= output_frames[f*file_header->height*file_header->width + (y+2)*file_header->width + x] << 2;
					pixel_data |= output_frames[f*file_header->height*file_header->width + (y+3)*file_header->width + x] << 3;
					pixel_data |= output_frames[f*file_header->height*file_header->width + (y+4)*file_header->width + x] << 4;
					pixel_data |= output_frames[f*file_header->height*file_header->width + (y+5)*file_header->width + x] << 5;
					pixel_data |= output_frames[f*file_header->height*file_header->width + (y+6)*file_header->width + x] << 6;
					pixel_data |= output_frames[f*file_header->height*file_header->width + (y+7)*file_header->width + x] << 7;
					printf("0x%X,", pixel_data);
				}
				printf("],\n");
			}
			printf("    ],\n\n");
		}
		printf("]\n");

    }
	else {
		printf("const unsigned char animation[%d][%d][%d] = {\n", file_header->frames, byte_height, file_header->width);
		for (int f = 0; f < file_header->frames; f++) {
			printf("    {\n");
			for (int y = 0; y < file_header->height; y+=8) {
				printf("        {");
				for (int x = 0; x < file_header->width; x++) {
					u8 pixel_data = 0;
					pixel_data |= output_frames[f*file_header->height*file_header->width + (y+0)*file_header->width + x] << 0;
					pixel_data |= output_frames[f*file_header->height*file_header->width + (y+1)*file_header->width + x] << 1;
					pixel_data |= output_frames[f*file_header->height*file_header->width + (y+2)*file_header->width + x] << 2;
					pixel_data |= output_frames[f*file_header->height*file_header->width + (y+3)*file_header->width + x] << 3;
					pixel_data |= output_frames[f*file_header->height*file_header->width + (y+4)*file_header->width + x] << 4;
					pixel_data |= output_frames[f*file_header->height*file_header->width + (y+5)*file_header->width + x] << 5;
					pixel_data |= output_frames[f*file_header->height*file_header->width + (y+6)*file_header->width + x] << 6;
					pixel_data |= output_frames[f*file_header->height*file_header->width + (y+7)*file_header->width + x] << 7;
					printf("0x%X,", pixel_data);
				}
				printf("},\n");
			}
			printf("    },\n\n");
		}
		printf("};\n");
	}

}
