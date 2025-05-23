#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

#define cast(t) (t)
#define array_count(a) (sizeof(a)/sizeof((a)[0]))

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t  i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef struct SliceU8 SliceU8;
struct SliceU8 {
	u64  len;
	u8  *data;
};

/////////////////
//~ fsize

// Sets EBADF if fp is not a seekable stream
// EINVAL if fp was NULL
static size_t
fsize(FILE *fp) {
	size_t fs = 0;
	
	if (fp) {
		fseek(fp, 0L, SEEK_END);
		
		if (errno == 0) {
			fs = ftell(fp);
			
			// If fseek succeeded before, it means that fp was
			// a seekable stream, so we don't check the error again.
			
			fseek(fp, 0L, SEEK_SET);
		}
	} else {
		errno = EINVAL;
	}
	
	return fs;
}

/////////////////
//~ File IO

typedef struct Read_File_Result Read_File_Result;
struct Read_File_Result {
	SliceU8 data;
	bool    valid;
};

static Read_File_Result
read_file_or_print_error(char *name) {
	Read_File_Result result = {0};
	
	FILE *handle = fopen(name, "rb");
	if (handle != 0) {
		errno = 0;
		i64 size = fsize(handle);
		if (errno == 0) {
			result.data.len  = size;
			result.data.data = malloc(size); // Caller is responsible for freeing
			i64 read_amount  = fread(result.data.data,
									 sizeof(u8),
									 result.data.len,
									 handle);
			if (read_amount == result.data.len || feof(handle)) {
				result.valid = true;
			} else {
				perror("fread");
			}
		} else {
			perror("fsize");
		}
		fclose(handle);
	} else {
		perror("fopen");
	}
	
	return result;
}

/////////////////
//~ Main

#define DEFAULT_BYTES_PER_ROW 16

int main(int argc, char **argv) {
	
	if (argc == 1) {
		printf("Prints a hexadecimal view of one or more files.\n"
			   "Usage:\n"
			   "  hexdump.exe [filename] [-help] [-width:<width>]\n");
	} else {
		
		bool help_mode = false;
		int  width = DEFAULT_BYTES_PER_ROW;
		
		int  file_count = 0;
		
		for (int argi = 1; argi < argc; argi += 1) {
			char *argn = argv[argi];
			if (argn[0] == '-') {
				char *flag = argn + 1;
				
				if (strcmp(flag, "help") == 0) {
					help_mode = true;
					
					// The -help flag has priority over everything. If it is specified,
					// nothing else matters.
					break;
				} else if (strcmp(flag, "width") == 0) {
					fprintf(stderr, "Unimplemented.\n");
				} else {
					fprintf(stderr, "Ignoring unknown flag '-%s'. Try '-help' for a list of possible flags.\n", flag);
				}
			} else {
				file_count += 1;
			}
		}
		
		if (help_mode) {
			printf("TODO: Help text\n");
		} else {
			// Main codepath: dump files.
			
			if (file_count == 0) {
				// No input files, but there were other arguments:
				// It's an error.
				
				fprintf(stderr, "No input files specified. Try again and specify at least one input file.\n");
			} else {
				char **file_names = malloc(file_count * sizeof(char *));
				assert(file_names);
				
				for (int argi = 1, file_index = 0; argi < argc; argi += 1) {
					char *argn = argv[argi];
					
					if (argn[0] != '-') {
						file_names[file_index] = argn;
						file_index += 1;
					}
				}
				
				for (int file_index = 0; file_index < file_count; file_index += 1) {
					char *name = file_names[file_index];
					printf("%s\n", name);
					
					Read_File_Result result = read_file_or_print_error(name);
					if (result.valid) {
						int bytes_per_row = 16;
						SliceU8 file = result.data;
						
						for (i64 offset = 0; offset < file.len; offset += bytes_per_row) {
							i64 eof    = min(offset + bytes_per_row, file.len);
							i64 amount = eof - offset;
							
							char buffer[1024];
							int  buffer_used = snprintf(buffer, array_count(buffer),
														"%08llx\t", offset);
							
							{
								for (i64 byte_index = 0; byte_index < amount; byte_index += 1) {
									assert(offset + byte_index < file.len);
									u8 b = file.data[offset + byte_index];
									
									buffer_used += snprintf(buffer + buffer_used, array_count(buffer) - buffer_used,
															"%02x ", b);
								}
								
								for (i64 byte_index = amount; byte_index < bytes_per_row; byte_index += 1) {
									buffer_used += snprintf(buffer + buffer_used, array_count(buffer) - buffer_used,
															"   ");
								}
								
								buffer_used += snprintf(buffer + buffer_used, array_count(buffer) - buffer_used,
														"\t");
							}
							
							{
								for (i64 byte_index = 0; byte_index < amount; byte_index += 1) {
									assert(offset + byte_index < file.len);
									u8 b = file.data[offset + byte_index];
									
									if (!isprint(b)) {
										b = ' ';
									}
									
									buffer_used += snprintf(buffer + buffer_used, array_count(buffer) - buffer_used,
															"%c", b);
								}
							}
							
							printf("%.*s\n", cast(int) array_count(buffer), buffer);
						}
						
						free(result.data.data);
					}
					
					printf("\n");
				}
				
				// free(file_names);
			}
		}
	}
	
	return 0;
}
