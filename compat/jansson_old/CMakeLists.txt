
include_directories(${PROJECT_SOURCE_DIR}/compat/jansson)

add_library(jansson	dump.c
			error.c
			hashtable.c
			hashtable_seed.c
			load.c
			memory.c
			pack_unpack.c
			strbuffer.c
			strconv.c
			utf.c
			value.c)