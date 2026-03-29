#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <pthread.h>
#include <signal.h>

#include "controller.h"
#include "worker.h"
#include "logger.h"


#define MAX_FILES 1000
#define MAX_PATH  512

void worker_process(int read_fd);

// File storage
char files[MAX_FILES][MAX_PATH];
int file_count = 0;

// Shared memory structure
typedef struct {
    int index;
    int total_files;
    char files[MAX_FILES][MAX_PATH];
} shm_data;

// Global semaphore (shared)
sem_t *sem;

// Scan directory
void scan_directory(const char *path) {
    struct dirent *entry;
    DIR *dir = opendir(path);

    if (!dir) {
        perror("opendir");
        exit(1);
    }

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            snprintf(files[file_count],
                     MAX_PATH,
                     "%s/%s",
                     path,
                     entry->d_name);
            file_count++;
        }
    }

    closedir(dir);
}

// Read instructions
void read_instructions(const char *filename) {
    FILE *fp = fopen(filename, "r");
    char line[512];

    if (!fp) {
        perror("fopen");
        exit(1);
    }

    printf("Instructions:\n");
    while (fgets(line, sizeof(line), fp)) {
        printf("%s", line);
    }

    fclose(fp);
}

// Signal handler
void handler(int sig) {
    printf("\nInterrupt received! Cleaning up...\n");
    shm_unlink("/file_shm");
    sem_unlink("/file_sem");
    exit(0);
}

void start_controller(int argc, char *argv[]) {

    if (argc != 3) {
        printf("Usage: %s <instructions.txt> <folder>\n", argv[0]);
        return;
    }

    signal(SIGINT, handler);

    // STEP 1: Read instructions
    read_instructions(argv[1]);

    // STEP 2: Scan directory
    scan_directory(argv[2]);

    printf("\nTotal files: %d\n", file_count);

    // STEP 3: Create pipe
    int fd[2];
    pipe(fd);

    // STEP 4: Shared Memory
    int shm_fd = shm_open("/file_shm", O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, sizeof(shm_data));

    shm_data *shm_ptr = mmap(0, sizeof(shm_data),
                             PROT_READ | PROT_WRITE,
                             MAP_SHARED, shm_fd, 0);

    // Store data in shared memory
    shm_ptr->index = 0;
    shm_ptr->total_files = file_count;

    for (int i = 0; i < file_count; i++) {
        strcpy(shm_ptr->files[i], files[i]);
    }

    // STEP 5: Semaphore
    sem = sem_open("/file_sem", O_CREAT, 0666, 1);

    // STEP 6: Create Workers
    int workers = 3;
    pid_t pids[workers];

    for (int i = 0; i < workers; i++) {
        pid_t pid = fork();

        if (pid == 0) {
            // Worker process
            close(fd[1]);          // close write end
            worker_process(fd[0]);
            exit(0);
        } else {
            pids[i] = pid;
        }
    }

    // Parent process
    close(fd[0]);

    // Send message to workers
    write(fd[1], "START_PROCESSING", strlen("START_PROCESSING"));
    close(fd[1]);

    // Wait for all workers using waitpid
    for (int i = 0; i < workers; i++) {
        waitpid(pids[i], NULL, 0);
    }

    printf("\nAll workers finished.\n");

    // Cleanup IPC
    shm_unlink("/file_shm");
    sem_unlink("/file_sem");
}
