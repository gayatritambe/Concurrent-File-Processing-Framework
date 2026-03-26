CC      = gcc
CFLAGS  = -Wall -Wextra -I./include
LDFLAGS = -lpthread -lrt

SRC = src/main.c src/controller.c src/worker.c \
      src/file_ops.c src/logger.c src/ipc.c

TARGET = filetool

all: $(TARGET)

$(TARGET):
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET) log.txt

debug:
	$(CC) $(CFLAGS) -g -fsanitize=address,thread $(SRC) -o $(TARGET) $(LDFLAGS)
