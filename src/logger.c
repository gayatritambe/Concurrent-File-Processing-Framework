#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include "logger.h"

void log_message(const char *msg) {
    int fd = open("log.txt", O_WRONLY | O_CREAT | O_APPEND, 0666);

    if (fd < 0) return;

    write(fd, msg, strlen(msg));
    write(fd, "\n", 1);

    close(fd);
}
