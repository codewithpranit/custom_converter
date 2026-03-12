CC = gcc
CFLAGS = -Wall -Wno-unused-function -O2
TARGET = converter

all: $(TARGET)

$(TARGET): converter.c
	$(CC) $(CFLAGS) -o $(TARGET) converter.c

clean:
	rm -f $(TARGET)

.PHONY: all clean
