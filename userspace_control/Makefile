CROSS_COMPILE = arm-none-linux-gnueabihf-
TARGET = dump_demo

CFLAGS = -static -g -Wall -std=gnu99
LDFLAGS = -g -Wall
CC = $(CROSS_COMPILE)gcc

build: $(TARGET)

$(TARGET): $(TARGET).o
	$(CC) $(LDFLAGS) $^ -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean

clean:
	rm -f $(TARGET) *.a *.o *.~

