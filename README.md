# 🗂️ FileTool — Multi-Process File Processor

> A Linux systems programming project in **C** demonstrating concurrent file processing  
> using **multi-processing**, **multi-threading**, and **POSIX IPC mechanisms**.

A controller process forks multiple worker processes that collaboratively process up to **1000 files** in a target directory — using shared memory, named semaphores, non-blocking pipes, and pthreads — while a live log tracks every operation.

---

## What This Project Does

FileTool takes a **folder of files** and an **instructions file**, then uses a fully concurrent pipeline to transform every file in that folder:

```
You give it  →  instructions.txt  +  ./my_folder  (up to 1000 files)

It does       →  adds a header line          ("Project2026")
                 replaces words              (TODO → DONE)
                 deletes tagged lines        (lines with DEBUG)
                 appends a completion line   ("Completed")

You get       →  all files transformed  +  log.txt tracking every operation
```

Every file is processed **concurrently** across 3 worker processes, each running 2 threads — no file is ever processed twice.

---
## Domain & Problem Context

This project is inspired by real-world **IoT and industrial data processing systems**, where large volumes of sensor or log data are continuously generated and must be processed efficiently.

In industries such as smart manufacturing, automotive systems, and IoT-based monitoring, multiple data sources generate files simultaneously. Processing these files sequentially can lead to performance bottlenecks and delays.

To address this, the **Concurrent File Processing Framework** uses:
- Multiple processes for parallel file handling
- Threads for efficient task execution
- IPC mechanisms for communication between components
- Signals for system control and lifecycle management

This architecture mimics real-world systems like:
- Industrial log processors
- IoT data aggregation systems
- Distributed monitoring pipelines

Thus, the project demonstrates how Linux system programming concepts are applied in practical, high-performance environments.
## Project Structure

```
filetool/
│
├── src/                          # All C source files
│   ├── main.c                    # Entry point — validates args, calls start_controller()
│   ├── controller.c              # Orchestrator — scans dir, sets up IPC, forks workers
│   ├── worker.c                  # Worker process — reads pipe, spawns threads
│   ├── file_ops.c                # The 4 file transformation functions
│   ├── logger.c                  # Appends entries to log.txt via write() syscall
│   └── ipc.c                     # IPC utility stubs (reserved for future helpers)
│
├── include/                      # All header files
│   ├── controller.h              # Declares start_controller()
│   ├── worker.h                  # Declares worker_process()
│   ├── file_ops.h                # Public API — all 4 file ops (include-guarded)
│   ├── logger.h                  # Declares log_message() + extern sem_t *sem
│   └── ipc.h                     # IPC header (placeholder)
│
├── obj/                          # Auto-created — compiled .o object files
├── log.txt                       # Auto-created at runtime — operation log
├── Makefile                      # Full build automation (see Build section)
└── README.md                     # This file
```

---

##  How It Works

### High-Level Flow

```
main()
  └── start_controller(argc, argv)
        │
        ├── 1. read_instructions()       reads and prints instructions.txt
        ├── 2. scan_directory()          collects all file paths → files[1000][512]
        ├── 3. pipe(fd)                  unnamed pipe for start signal
        ├── 4. shm_open() + mmap()       shared memory — file list + shared index
        ├── 5. sem_open("/file_sem")     named semaphore — guards the shared index
        │
        └── 6. fork() × 3
                └── worker_process(read_fd)
                      │
                      ├── shm_open()         attaches to /file_shm
                      ├── sem_open()         attaches to /file_sem
                      ├── read(pipe)         non-blocking read with EAGAIN loop
                      │                      waits for "START_PROCESSING"
                      │
                      ├── pthread_create(t1) → process_files()
                      └── pthread_create(t2) → logger_thread()
```

### The Critical Section — How Files Are Distributed

Each worker thread picks up files using an **atomic index grab** protected by the semaphore:

```c
sem_wait(sem);               // lock
  int i = shm->index;        // read current file index
  shm->index++;              // claim it
sem_post(sem);               // unlock

process( shm->files[i] );    // work on your file — outside the lock
```

This guarantees:
- No two workers ever touch the same file
- All 1000 files are processed — none skipped, none doubled
- Workers run truly in parallel outside the critical section

### Pipe — Non-Blocking with EAGAIN

The pipe read in `worker.c` uses `O_NONBLOCK` to prevent workers from hanging:

```c
fcntl(read_fd, F_SETFL, flags | O_NONBLOCK);

while (1) {
    int n = read(read_fd, buffer, sizeof(buffer) - 1);
    if (n > 0)  { /* got signal, proceed */ break; }
    if (n == 0) { break; }   // pipe closed
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        usleep(10000);        // nothing yet — wait and retry
        continue;
    }
}
```

This was a known blocking issue in an earlier version — now fully resolved.

---

##  Concurrency Model

```
┌─────────────────────────────────────────────────┐
│            Controller Process (parent)           │
│  scan → setup IPC → write pipe → waitpid × 3    │
└───────────┬─────────────┬────────────┬───────────┘
            │             │            │
     fork() │      fork() │     fork() │
            ▼             ▼            ▼
    ┌──────────────┐  ┌──────────────┐  ┌──────────────┐
    │   Worker 1   │  │   Worker 2   │  │   Worker 3   │
    │              │  │              │  │              │
    │ Thread 1 ──► │  │ Thread 1 ──► │  │ Thread 1 ──► │
    │ process_files│  │ process_files│  │ process_files│
    │              │  │              │  │              │
    │ Thread 2 ──► │  │ Thread 2 ──► │  │ Thread 2 ──► │
    │ logger_thread│  │ logger_thread│  │ logger_thread│
    └──────┬───────┘  └──────┬───────┘  └──────┬───────┘
           │                 │                  │
           └────────┬────────┘                  │
                    └──────────────┬────────────┘
                                   ▼
                     ┌─────────────────────────┐
                     │   /file_shm  (shared)   │
                     │   index  →  0..999      │
                     │   files[] → 1000 paths  │
                     └─────────────────────────┘
                     ┌─────────────────────────┐
                     │   /file_sem  (semaphore) │
                     │   guards shm->index      │
                     └─────────────────────────┘
```

**3 processes × 2 threads = 6 concurrent workers** racing through the file list.  
The parent uses `waitpid()` (not `wait()`) to track each child by PID reliably.

---

##  IPC Mechanisms

| Mechanism | Resource | Purpose | Where Used |
|---|---|---|---|
| `pipe()` | `fd[0]` / `fd[1]` | Controller signals workers to begin | `controller.c` → `worker.c` |
| `shm_open()` + `mmap()` | `/file_shm` | Shares file list + index across all processes | `controller.c`, `worker.c` |
| `sem_open()` | `/file_sem` | Mutual exclusion on `shm->index` | `worker.c` threads |
| `fork()` | — | Creates 3 independent worker processes | `controller.c` |
| `pthread_create()` | — | 2 threads per worker (processor + logger) | `worker.c` |
| `signal(SIGINT, handler)` | — | Cleans up IPC on Ctrl+C | `controller.c` |

### The `shm_data` Struct — Must Match in Both Files

The shared memory layout is defined identically in both `controller.c` and `worker.c`:

```c
#define MAX_FILES 1000
#define MAX_PATH  512

typedef struct {
    int  index;                        // next file to claim
    int  total_files;                  // total files found
    char files[MAX_FILES][MAX_PATH];   // full path of each file
} shm_data;
```

> ⚠️ If `MAX_PATH` ever differs between `controller.c` and `worker.c`, memory layout breaks silently.  
> Both now correctly use `512` — this was a bug in an earlier version where worker used `256`.

---

## File Operations

Each file goes through all **4 operations in sequence**, handled by `file_ops.c`.  
All operations use a **write-to-temp → `rename()`** strategy — the original file is never partially written:

```
original.txt
    │
    ├── open for read
    ├── write modified content → original.txt.tmp
    ├── fclose both
    ├── remove(original.txt)
    └── rename(.tmp → original.txt)   ← atomic on Linux
```

| Step | Function | What Happens |
|---|---|---|
| 1 | `add_header(file, "Project2026")` | `"Project2026"` becomes the very first line |
| 2 | `append_line(file, "Completed")` | `"Completed"` is added as the very last line |
| 3 | `replace_word(file, "TODO", "DONE")` | Every occurrence of `TODO` on every line → `DONE` |
| 4 | `delete_line(file, "DEBUG")` | Any line containing `DEBUG` is removed entirely |

**Before and after example:**

```
BEFORE                          AFTER
──────────────────────          ──────────────────────
Line one                        Project2026
TODO fix this function          Line one
DEBUG checkpoint here           DONE fix this function
                                Completed
```

---

## Public API

### `controller.h`
```c
void start_controller(int argc, char *argv[]);
```
The main orchestrator. Called directly from `main()`.  
`argv[1]` = instructions file path, `argv[2]` = target folder path.

---

### `file_ops.h`
```c
void add_header(const char *filename, const char *header);
void append_line(const char *filename, const char *line);
void replace_word(const char *filename, const char *old, const char *new);
void delete_line(const char *filename, const char *keyword);
```
The only header with full `#ifndef` / `#define` / `#endif` include guards.  
All four transformation functions operate on a file path and use the temp-rename pattern.

---

### `logger.h`
```c
extern sem_t *sem;
void log_message(const char *msg);
```
`log_message()` appends `msg + "\n"` to `log.txt` using raw `write()` — no stdio buffering,  
so log entries are flushed to disk immediately, safe across concurrent processes.  
`sem` is declared `extern` here and owned by `controller.c`; workers inherit it via `fork()`.

---

### `worker.h`
```c
void worker_process(int read_fd);
```
Called in the child process after `fork()`. Takes the pipe read-end fd, attaches to shared memory and semaphore, then spawns the two threads.

---

### `ipc.h` / `ipc.c`
Currently placeholder stubs. Intended future home for reusable helpers:  
pipe setup/teardown, `shm_data` init/cleanup, semaphore open/close wrappers.

---

##  Getting Started

### Prerequisites

| Requirement | Details |
|---|---|
| **OS** | Linux — Ubuntu 20.04 or later recommended |
| **Compiler** | GCC 9+ (`gcc --version` to check) |
| **Libraries** | `libpthread`, `librt` — both standard on any Linux install |
| **Make** | `make` — optional but strongly recommended |

No external dependencies. Everything used (`pthread`, `semaphore.h`, `sys/mman.h`, `fcntl.h`) is part of the POSIX standard and ships with GCC on Linux.

---

## Build Instructions

### Option A — Makefile (Recommended)

```bash
# Standard build
make

# Debug build — adds -g, O0, ThreadSanitizer
make debug

# Optimized release build
make release

# Remove binary, obj/, log.txt
make clean

# Remove stale /dev/shm IPC objects after a crash
make clean-ipc

# Full reset — clean + clean-ipc
make purge
```

### Option B — Manual GCC

```bash
gcc -Wall -Wextra -I./include \
    src/main.c src/controller.c src/worker.c \
    src/file_ops.c src/logger.c src/ipc.c \
    -o filetool -lpthread -lrt
```

> **Why `-lpthread`?** Required for `pthread_create()`, `sem_open()`, `sem_wait()`.  
> **Why `-lrt`?** Required for `shm_open()`, `shm_unlink()` on older glibc (safe to always include).  
> **Why `-I./include`?** Tells GCC where to find all `.h` header files.

---

## Running the Program

```bash
./filetool <instructions.txt> <target_folder>
```

| Argument | Description |
|---|---|
| `instructions.txt` | A plain text file — its contents are printed at startup |
| `target_folder` | The directory whose files will be processed (non-recursive) |

**Example:**

```bash
./filetool instructions.txt ./my_files
```

### Example Output

```
Instructions:
Process all files in folder
Replace TODO markers and remove DEBUG lines

Total files: 1000

Worker received: START_PROCESSING
Processing: ./my_files/report_001.txt
Done: ./my_files/report_001.txt
Processing: ./my_files/notes_042.txt
Done: ./my_files/notes_042.txt
Processing: ./my_files/data_087.txt
Done: ./my_files/data_087.txt
...
All workers finished.
```

Lines will appear interleaved from all 3 workers — that is normal and expected.

---

## Logging & Monitoring

Every completed file is recorded in **`log.txt`** in your working directory:

```
File processed
File processed
File processed
...
```

`log.txt` is created automatically. Each entry is written with `O_APPEND` + raw `write()` — no stdio buffering — so entries hit the file immediately as each file completes.

**Watch progress live in a second terminal:**

```bash
tail -f log.txt
```

With 1000 files and 3 workers running 2 threads each, you will see entries streaming in fast.  
Count completed files at any point with:

```bash
wc -l log.txt
```

---

##  Signal Handling

Press `Ctrl+C` at any time to stop cleanly:

```
^C
Interrupt received! Cleaning up...
```

The `SIGINT` handler runs:

```c
shm_unlink("/file_shm");    // destroy shared memory object
sem_unlink("/file_sem");    // destroy named semaphore
exit(0);
```

**If the program crashes** before the handler runs, stale IPC objects stay in `/dev/shm/`.  
You will see `shm_open: File exists` on the next run. Fix it with:

```bash
# Via Makefile
make clean-ipc

# Or manually
rm /dev/shm/file_shm
rm /dev/shm/sem.file_sem
```

Check what is lingering at any time:

```bash
ls /dev/shm/
```

---


##  Future Improvements

- [ ] Add `clock_gettime()` timestamps to every `log.txt` entry
- [ ] Add error handling for `pthread_create()`, `mmap()`, `shm_open()`, `sem_open()`
- [ ] Make worker count configurable via CLI flag — `./filetool -w 5 instructions.txt ./folder`
- [ ] Replace fixed `MAX_FILES` / `MAX_PATH` with dynamic allocation

---
