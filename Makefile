CC = gcc
# Add -D_GNU_SOURCE to CFLAGS to ensure feature macros
CFLAGS = -Wall -Wextra -std=c99 -g -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L
TARGET = yash
SRC = src/shell.c
OBJ = $(SRC:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

%.o: %.c include/shell.h
	$(CC) $(CFLAGS) -Iinclude -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run