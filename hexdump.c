#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
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
	i64  len;
	u8  *data;
};

typedef struct String String;
struct String {
	i64  len;
	u8  *data;
};

static String
string(u8 *data, i64 len) {
	String result = {
		.data = data,
		.len  = len,
	};
	return result;
}

#define string_lit_expand(s)   s, (sizeof(s)-1)
#define string_expand(s)       cast(int)(s).len, (s).data

#define string_from_lit(s)     string(cast(u8 *)s, sizeof(s)-1)
#define string_from_cstring(s) string(cast(u8 *)s, strlen(s))

#define string_equals(a, b)    (((a).len == (b).len) && (memcmp((a).data, (b).data, (a).len) == 0))

static bool
string_starts_with(String a, String b) {
	bool result = false;
	if (a.len >= b.len) {
		int memcmp_result = memcmp(a.data, b.data, b.len);
		result = memcmp_result == 0;
	}
	return result;
}

static String
string_skip(String s, i64 amount) {
	if (amount > s.len) {
		amount = s.len;
	}
	
	s.data += amount;
	s.len  -= amount;
	
	return s;
}

static String
string_chop(String s, i64 amount) {
	if (amount > s.len) {
		amount = s.len;
	}
	
	s.len -= amount;
	
	return s;
}

static String
string_chop_at(String s, i64 index) {
	if (index < s.len && index > -1) {
		s.len = index;
	}
	
	return s;
}

static i64
string_find_first(String s, u8 c) {
	i64 result = -1;
	for (i64 i = 0; i < s.len; i += 1) {
		if (s.data[i] == c) {
			result = i;
			break;
		}
	}
	return result;
}

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
			result.data.data = malloc(size * sizeof(u8)); // Caller is responsible for freeing
			assert(result.data.data);
			i64 read_amount  = fread(result.data.data,
									 sizeof(u8),
									 result.data.len,
									 handle);
			if (read_amount == result.data.len || feof(handle)) {
				result.valid = true;
			} else {
				fprintf(stderr, "Could not read the file '%s': %s", name, strerror(errno));
			}
		} else {
			fprintf(stderr, "Could not read the file '%s': %s", name, strerror(errno));
			
			// Actually we didn't even try to read the file, we just failed to query its size,
			// but the user doesn't care about this since it's an implementation detail.
		}
		fclose(handle);
	} else {
		fprintf(stderr, "Could not open the file '%s': %s", name, strerror(errno));
	}
	
	return result;
}

/////////////////
//~ Main

#define DEFAULT_BYTES_PER_ROW 16

#define     MAX_BYTES_PER_ROW 64
#define     MIN_BYTES_PER_ROW  1

#define INFO_TEXT \
"Prints a hexadecimal view of one or more files.\n" \
"Usage:\n" \
"  hexdump.exe [filename] [-help] [-width:<width>]\n"

#define HELP_TEXT \
"  -help            display this text\n" \
"  -width:<width>   set how many bytes are printed per row\n" \
"     Example: '-width:12'\n" \
"\n" \
"Example:\n" \
"  hexdump.exe my_file.txt my_other_file.png -width:12\n"

int main(int argc, char **argv) {
	
	bool args_ok = true;
	
	bool info_mode = false;
	bool help_mode = false;
	int  width = -1;
	
	int  file_count = 0;
	
	if (argc <= 1) { // Does it make sense to check for less-than? (Imagine a program invoking the process through API call passing the wrong arguments)
		info_mode = true;
	} else {
		for (int argi = 1; argi < argc; argi += 1) {
			String argn = string_from_cstring(argv[argi]);
			if (argn.data[0] != '-') {
				file_count += 1;
			} else {
				i64 sep_index = string_find_first(argn, ':');
				String flag   = string_skip(string_chop_at(argn, sep_index >= 0 ? sep_index : argn.len), 1);
				String rest   = string_skip(argn, sep_index >= 0 ? sep_index + 1 : argn.len);
				
				if (string_equals(flag, string_from_lit("help"))) {
					help_mode = true;
					
					// The -help flag has priority over everything. If it is specified,
					// nothing else matters.
					break;
				} else if (string_equals(flag, string_from_lit("width"))) {
					if (width != -1) {
						fprintf(stderr, "Ignoring repeated flag '-width'.\n");
					} else if (rest.len > 0) {
						
						bool width_parsing_ok = true;
						
						char sign_char = ' ';
						if (rest.data[0] == '+' || rest.data[0] == '-') {
							sign_char = rest.data[0];
							rest = string_skip(rest, 1);
							
							if (rest.len == 0) {
								fprintf(stderr, "The argument for '-width' must be an integer number, e.g. '-width:12'.\n");
								width_parsing_ok = false;
							}
						}
						
						if (width_parsing_ok) {
							for (int char_index = 0; char_index < rest.len; char_index += 1) {
								if (!isdigit(rest.data[char_index])) {
									fprintf(stderr, "The argument for '-width' must be an integer number, e.g. '-width:12'.\n");
									width_parsing_ok = false;
									break;
								}
							}
						}
						
						if (width_parsing_ok) {
							if (sign_char == '-') {
								fprintf(stderr, "The argument for '-width' must be a positive number, e.g. '-width:12'.\n");
								width_parsing_ok = false;
							}
						}
						
						int width_arg = 0;
						if (width_parsing_ok) {
							for (int char_index = 0; char_index < rest.len; char_index += 1) {
								char digit = rest.data[char_index] - '0';
								if ((width_arg <= (MAX_BYTES_PER_ROW / 10)) &&
									(width_arg <  (MAX_BYTES_PER_ROW / 10)) || digit <= (MAX_BYTES_PER_ROW % 10)) {
									width_arg *= 10;
									width_arg += digit;
								} else {
									fprintf(stderr, "The argument %.*s for '-width' is too big, the max supported is %d.\n", string_expand(rest), MAX_BYTES_PER_ROW);
									width_parsing_ok = false;
									break;
								}
							}
							if (width_arg < MIN_BYTES_PER_ROW) {
								fprintf(stderr, "The argument %.*s for '-width' is too small, the min supported is %d.\n", string_expand(rest), MIN_BYTES_PER_ROW);
								width_parsing_ok = false;
							}
						}
						
						if (width_parsing_ok) {
							width = width_arg;
						} else {
							args_ok = false;
							break;
						}
						
					} else {
						fprintf(stderr, "Flag '-width' requires an integer argument, e.g. '-width:12'.\n");
						args_ok = false;
						break;
					}
				} else {
					fprintf(stderr, "Ignoring unknown flag '-%.*s'. Try '-help' for a list of possible flags.\n", string_expand(flag)); // TODO: "Did you mean ...?"
				}
			}
		}
	}
	
	if (info_mode) {
		printf(INFO_TEXT);
	} else if (help_mode) {
		printf(INFO_TEXT "\n" HELP_TEXT);
	} else if (args_ok) {
		// Main codepath: dump files.
		
		if (width == -1) {
			width = DEFAULT_BYTES_PER_ROW;
		}
		
		assert(width > 0);
		
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
				
				Read_File_Result result = read_file_or_print_error(name);
				if (result.valid) {
					int bytes_per_row = width;
					SliceU8 file = result.data;
					
					printf("%s\n", name);
					
					for (i64 offset = 0; offset < file.len; offset += bytes_per_row) {
						i64 eof    = min(offset + bytes_per_row, file.len);
						i64 amount = eof - offset;
						
						// The buffer needs to be big enough to hold:
						//   8 + \t + (2*MAX_BYTES_PER_ROW + MAX_BYTES_PER_ROW-1) + \t + MAX_BYTES_PER_ROW
						
						char buffer[1024];
						int  buffer_used = snprintf(buffer, array_count(buffer),
													"%08llx\t", offset); // Print address
						
						{
							// Print hex representation of the bytes
							
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
							// Print ASCII representation of the bytes
							
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
	
	return 0;
}
