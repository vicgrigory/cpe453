# Flags
CC = gcc
CFLAGS = -Wall -g -fpic -pedantic -ansi
BUILD = build


all: prepare minget minls

# Main function builds
minget: $(BUILD)/min_get.o $(BUILD)/min_parse.o $(BUILD)/min_filesystem.o $(BUILD)/min_common.o
	$(CC) $(BUILD)/min_get.o $(BUILD)/min_parse.o $(BUILD)/min_filesystem.o $(BUILD)/min_common.o -o minget
minls: $(BUILD)/min_ls.o $(BUILD)/min_parse.o $(BUILD)/min_filesystem.o $(BUILD)/min_common.o
	$(CC) $(BUILD)/min_ls.o $(BUILD)/min_parse.o $(BUILD)/min_filesystem.o $(BUILD)/min_common.o -o minls

# Support function builds
$(BUILD)/min_get.o: min_get.c min.h
	$(CC) $(CFLAGS) min_get.c -o $(BUILD)/min_get.o -c

$(BUILD)/min_ls.o: min_ls.c min.h
	$(CC) $(CFLAGS) min_ls.c -o $(BUILD)/min_ls.o -c

$(BUILD)/min_parse.o: min_parse.c min.h
	$(CC) $(CFLAGS) min_parse.c -o $(BUILD)/min_parse.o -c

$(BUILD)/min_filesystem.o: min_filesystem.c min.h
	$(CC) $(CFLAGS) min_filesystem.c -o $(BUILD)/min_filesystem.o -c

$(BUILD)/min_common.o: min_common.c min.h
	$(CC) $(CFLAGS) min_common.c -o $(BUILD)/min_common.o -c

# Miscellanous builds
prepare:
	mkdir -p $(BUILD)

tidy:
	rm -rf $(BUILD)

clean: tidy
	rm -rf minls minget