/**
 * @file ptrace_attach.c
 * @brief Implementation of ptrace attachment functions
 *
 * This file implements functions for attaching to processes using the
 * ptrace API, managing threads, and accessing process memory for
 * deadlock detection purposes.
 */

#include "ptrace_attach.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <sys/syscall.h>
#include <unistd.h>

// Print debugging information if enabled
#define DEBUG 1
#define debug_print(fmt, ...) \
    do { if (DEBUG) fprintf(stderr, "[PTRACE] " fmt, ##__VA_ARGS__); } while (0)

bool ptrace_attach(pid_t pid) {
    if (ptrace(PTRACE_ATTACH, pid, NULL, NULL) == -1) {
        perror("ptrace attach");
        return false;
    }

    int status;
    if (waitpid(pid, &status, 0) == -1) {
        perror("waitpid after attach");
        return false;
    }

    // Set PTRACE_O_TRACESYSGOOD to distinguish syscall stops from other stops
    if (ptrace(PTRACE_SETOPTIONS, pid, NULL, PTRACE_O_TRACESYSGOOD) == -1) {
        perror("ptrace setoptions");
        return false;
    }

    debug_print("Successfully attached to process %d\n", pid);
    return true;
}

bool ptrace_detach(pid_t pid) {
    if (ptrace(PTRACE_DETACH, pid, NULL, NULL) == -1) {
        perror("ptrace detach");
        return false;
    }

    debug_print("Successfully detached from process %d\n", pid);
    return true;
}

/**
 * Helper function to get thread IDs for a process
 */
static pid_t* get_thread_ids(pid_t pid, int* count) {
    char task_path[64];
    snprintf(task_path, sizeof(task_path), "/proc/%d/task", pid);

    DIR* dir = opendir(task_path);
    if (!dir) {
        perror("opendir");
        return NULL;
    }

    // First pass: count threads
    struct dirent* entry;
    *count = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] != '.') {
            (*count)++;
        }
    }

    // Allocate array for thread IDs
    pid_t* thread_ids = malloc(*count * sizeof(pid_t));
    if (!thread_ids) {
        perror("malloc");
        closedir(dir);
        return NULL;
    }

    // Second pass: collect thread IDs
    rewinddir(dir);
    int i = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] != '.') {
            thread_ids[i++] = atoi(entry->d_name);
        }
    }

    closedir(dir);
    return thread_ids;
}

int ptrace_attach_all_threads(pid_t pid) {
    int thread_count;
    pid_t* thread_ids = get_thread_ids(pid, &thread_count);
    if (!thread_ids) {
        return -1;
    }

    debug_print("Found %d threads for process %d\n", thread_count, pid);

    int success_count = 0;
    for (int i = 0; i < thread_count; i++) {
        pid_t tid = thread_ids[i];

        // Skip main thread if already attached
        if (tid == pid) {
            success_count++;
            continue;
        }

        if (ptrace_attach(tid)) {
            success_count++;
        } else {
            debug_print("Failed to attach to thread %d\n", tid);
        }
    }

    free(thread_ids);
    return success_count;
}

long ptrace_wait_for_syscall(pid_t pid, bool* entering) {
    int status;
    long syscall;

    while (1) {
        // Restart the process until the next system call entry or exit
        if (ptrace(PTRACE_SYSCALL, pid, NULL, NULL) == -1) {
            perror("ptrace syscall");
            return -1;
        }

        // Wait for the process to stop
        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid");
            return -1;
        }

        // Check if the process has exited
        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            debug_print("Process %d has terminated\n", pid);
            return -1;
        }

        // Check if this is a system call stop
        if (WIFSTOPPED(status) && WSTOPSIG(status) == (SIGTRAP | 0x80)) {
            syscall = ptrace_get_syscall_nr(pid);

            // Check if we're entering or exiting the syscall
            // This is architecture-dependent, but on x86_64:
            // - When entering, the syscall number is in the orig_rax register
            // - When exiting, the syscall number is in the orig_rax register and the return value is in rax

            struct user_regs_struct regs;
            if (ptrace(PTRACE_GETREGS, pid, NULL, &regs) == -1) {
                perror("ptrace getregs");
                return -1;
            }

            // On syscall entry, rax is the syscall number
            // On syscall exit, rax is the return value
            if (entering != NULL) {
                *entering = (regs.rax == (unsigned long long) (-ENOSYS));
            }

            return syscall;
        }
    }
}

long ptrace_get_syscall_nr(pid_t pid) {
    struct user_regs_struct regs;
    if (ptrace(PTRACE_GETREGS, pid, NULL, &regs) == -1) {
        perror("ptrace getregs");
        return -1;
    }

    // On x86_64, orig_rax holds the syscall number
    return regs.orig_rax;
}

unsigned long ptrace_get_syscall_arg(pid_t pid, int arg_index) {
    if (arg_index < 0 || arg_index > 5) {
        fprintf(stderr, "Invalid syscall argument index: %d\n", arg_index);
        return 0;
    }

    struct user_regs_struct regs;
    if (ptrace(PTRACE_GETREGS, pid, NULL, &regs) == -1) {
        perror("ptrace getregs");
        return 0;
    }

    // On x86_64, syscall arguments are passed in registers:
    // rdi, rsi, rdx, r10, r8, r9 for arguments 0-5
    switch (arg_index) {
        case 0: return regs.rdi;
        case 1: return regs.rsi;
        case 2: return regs.rdx;
        case 3: return regs.r10;
        case 4: return regs.r8;
        case 5: return regs.r9;
        default: return 0; // Should not happen due to check above
    }
}

long ptrace_get_syscall_result(pid_t pid) {
    struct user_regs_struct regs;
    if (ptrace(PTRACE_GETREGS, pid, NULL, &regs) == -1) {
        perror("ptrace getregs");
        return 0;
    }

    // On x86_64, rax holds the syscall return value
    return regs.rax;
}

bool ptrace_read_memory(pid_t pid, unsigned long addr, void* data, size_t len) {
    // Read memory word by word (a word is sizeof(long))
    unsigned long* dst = (unsigned long*)data;
    const size_t word_size = sizeof(long);
    size_t words = (len + word_size - 1) / word_size; // Round up to whole words

    for (size_t i = 0; i < words; i++) {
        unsigned long word = ptrace(PTRACE_PEEKDATA, pid, addr + i * word_size, NULL);
        if (word == (unsigned long long)-1 && errno != 0) {
            perror("ptrace peekdata");
            return false;
        }

        // For the last word, we might need to copy only part of it
        if (i == words - 1 && len % word_size != 0) {
            memcpy(&dst[i], &word, len % word_size);
        } else {
            dst[i] = word;
        }
    }

    return true;
}

bool ptrace_write_memory(pid_t pid, unsigned long addr, const void* data, size_t len) {
    // Write memory word by word (a word is sizeof(long))
    const unsigned long* src = (const unsigned long*)data;
    const size_t word_size = sizeof(long);
    size_t words = (len + word_size - 1) / word_size; // Round up to whole words

    for (size_t i = 0; i < words; i++) {
        unsigned long word;

        // For the last word, we might need to preserve part of the original memory
        if (i == words - 1 && len % word_size != 0) {
            // Read the original word
            word = ptrace(PTRACE_PEEKDATA, pid, addr + i * word_size, NULL);
            if (word == (unsigned long long) -1 && errno != 0) {
                perror("ptrace peekdata");
                return false;
            }

            // Modify only the necessary bytes
            memcpy(&word, &src[i], len % word_size);
        } else {
            word = src[i];
        }

        // Write the word back
        if (ptrace(PTRACE_POKEDATA, pid, addr + i * word_size, word) == -1) {
            perror("ptrace pokedata");
            return false;
        }
    }

    return true;
}
