#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include "file_ops.h"
#include "logger.h"

// Shared memory structure — MUST match controller.c exactly
#define MAX_FILES 1000
#define MAX_PATH  512

typedef struct {
    int index;
    int total_files;
    char files[MAX_FILES][MAX_PATH];  
} shm_data;


// Thread 1: Process files
void *process_files(void *arg) {
    shm_data *shm = (shm_data *)arg;

    while (1) {
        sem_wait(sem);

        if (shm->index >= shm->total_files) {
            sem_post(sem);
            break;
        }

        int i = shm->index;
        shm->index++;

        sem_post(sem);

        // Get file name
        char *file = shm->files[i];

        printf("Processing: %s\n", file);

        // File operations
        add_header(file, "Project2026");
        append_line(file, "Completed");
        replace_word(file, "TODO", "DONE");
        delete_line(file, "DEBUG");

        printf("Done: %s\n", file);

        // Logging
        log_message("File processed");
    }

    return NULL;
}

// Thread 2: Logger thread
void *logger_thread(void *arg) {
    printf("Logger thread running...\n");
    return NULL;
}

// Worker main function
void worker_process(int read_fd) {

    // Open shared memory
    int shm_fd = shm_open("/file_shm", O_RDWR, 0666);
    shm_data *shm_ptr = mmap(0, sizeof(shm_data),
                             PROT_READ | PROT_WRITE,
                             MAP_SHARED, shm_fd, 0);

    // Open semaphore
    sem = sem_open("/file_sem", 0);

    // FIX: set pipe to non-blocking so workers don't hang
    int flags = fcntl(read_fd, F_GETFL, 0);
    fcntl(read_fd, F_SETFL, flags | O_NONBLOCK);

    // Read from pipe
    char buffer[512];

	while (1) {
    	int n = read(read_fd, buffer, sizeof(buffer) - 1);

    	if (n > 0) {
        buffer[n] = '\0';

    	    char *token = strtok(buffer, "\n");
 	     	  while (token != NULL) {
 	           printf("Worker received file: %s\n", token);
	            token = strtok(NULL, "\n");
        }
    } 
    else if (n == 0) {
        break;
    } 
    else {
     	   if (errno == EAGAIN || errno == EWOULDBLOCK) {
     	       usleep(10000);
    	        continue;
    	    } else {
            perror("read");
            break;
   	     }
  	  }
	}

close(read_fd);

    // Create threads
    pthread_t t1, t2;

    pthread_create(&t1, NULL, process_files, shm_ptr);
    pthread_create(&t2, NULL, logger_thread, NULL);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
}
