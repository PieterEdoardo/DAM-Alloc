CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -Iinclude

SRC = \
    tests/test.c \
	src/core/dam_core.c \
	src/util/dam_util.c \

bin/test: $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o bin/test
