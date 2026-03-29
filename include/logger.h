#ifndef LOGGER_H
#define LOGGER_H
#include <semaphore.h>

extern sem_t *sem; 
void log_message(const char *msg);

#endif
