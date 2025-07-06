/**
 * @file backtrace.c
 * @brief Implementation of backtrace analysis for ptrace-based deadlock detection
 *
 * This file implements functions for capturing and analyzing stack traces
 * from threads in a traced process using ptrace, allowing for identification
 * of threads blocked in mutex operations.
 */

#include "backtrace.h"
#include "ptrace_attach.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <limits.h>
#include <elf.h>
#include <link.h>
#include <dlfcn.h>

// Print debugging information if enabled
#define DEBUG 1
#define debug_print(fmt, ...) \
    do { if (DEBUG) fprintf(stderr, "[BACKTRACE] " fmt, ##__VA_ARGS__); } while (0)

// Flag for using debug symbols
static bool use_debug_symbols = true;

// Path to the executable of the target process
static char executable_path[PATH_MAX] = {0};

// Structure to store symbol information
typedef struct {
    void* addr;                   // Symbol address
    unsigned long size;           // Symbol size
    char name[MAX_SYMBOL_LENGTH]; // Symbol name
} symbol_info_t;

/**
 * Helper function to get the executable path for a process
 */
static bool get_process_executable(pid_t pid, char* path, size_t max_len) {
    char proc_exe[PATH_MAX];
    snprintf(proc_exe, sizeof(proc_exe), "/proc/%d/exe", pid);
    
    ssize_t len = readlink(proc_exe, path, max_len - 1);
    if (len < 0) {
        perror("readlink");
        return false;
    }
    
    path[len] = '\0';
    return true;
}

/**
 * Helper function to read a value from a register
 */
static unsigned long get_register_value(pid_t pid, int reg) {
    struct user_regs_struct regs;
    if (ptrace(PTRACE_GETREGS, pid, NULL, &regs) == -1) {
        perror("ptrace getregs");
        return 0;
    }
    
    // Map register index to the appropriate register
    switch (reg) {
        case 0: return regs.rax;
        case 1: return regs.rbx;
        case 2: return regs.rcx;
        case 3: return regs.rdx;
        case 4: return regs.rsi;
        case 5: return regs.rdi;
        case 6: return regs.rbp;
        case 7: return regs.rsp;
        case 8: return regs.r8;
        case 9: return regs.r9;
        case 10: return regs.r10;
        case 11: return regs.r11;
        case 12: return regs.r12;
        case 13: return regs.r13;
        case 14: return regs.r14;
        case 15: return regs.r15;
        case 16: return regs.rip;
        default: return 0;
    }
}

/**
 * Helper function to find a symbol in the executable or shared libraries
 */
static bool find_symbol_info(void* addr, symbol_info_t* info) {
    if (!use_debug_symbols || executable_path[0] == '\0') {
        return false;
    }
    
    // Initialize info with defaults
    info->addr = addr;
    info->size = 0;
    snprintf(info->name, MAX_SYMBOL_LENGTH, "??");
    
    // This implementation is a placeholder. A real implementation would:
    // 1. Parse the ELF file and its symbol table
    // 2. Check for debug info (DWARF)
    // 3. Handle shared libraries
    // 4. Use addr2line or similar functionality
    
    // For a basic implementation, we could use system tools like addr2line:
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "addr2line -e %s -f -s -p %p", executable_path, addr);
    
    FILE* pipe = popen(cmd, "r");
    if (!pipe) {
        return false;
    }
    
    char line[MAX_SYMBOL_LENGTH];
    if (fgets(line, sizeof(line), pipe) != NULL) {
        // Remove trailing newline
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }
        
        // Copy the symbol name
        strncpy(info->name, line, MAX_SYMBOL_LENGTH - 1);
        info->name[MAX_SYMBOL_LENGTH - 1] = '\0';
    }
    
    pclose(pipe);
    return true;
}

/**
 * Helper function to unwind the stack using frame pointers
 */
static int unwind_stack_fp(pid_t pid, void** frames, int max_frames) {
    unsigned long rbp, rip, rsp;
    int frame_count = 0;
    
    // Get initial values of registers
    rbp = get_register_value(pid, 6);  // RBP
    rip = get_register_value(pid, 16); // RIP
    rsp = get_register_value(pid, 7);  // RSP
    
    if (rbp == 0 || rip == 0 || rsp == 0) {
        debug_print("Failed to get initial register values\n");
        return 0;
    }
    
    // Store the current instruction pointer as the first frame
    if (frame_count < max_frames) {
        frames[frame_count++] = (void*)rip;
    }
    
    // Walk the stack using frame pointers
    while (frame_count < max_frames && rbp != 0) {
        // The return address is stored at rbp+8
        unsigned long next_rip, next_rbp;
        if (!ptrace_read_memory(pid, rbp + 8, &next_rip, sizeof(next_rip)) ||
            !ptrace_read_memory(pid, rbp, &next_rbp, sizeof(next_rbp))) {
            break;
        }
        
        // Check for obviously invalid values
        if (next_rip == 0 || next_rbp <= rbp) {
            break;
        }
        
        frames[frame_count++] = (void*)next_rip;
        rbp = next_rbp;
    }
    
    return frame_count;
}

bool backtrace_init(void) {
    // Nothing to initialize at this point
    return true;
}

void backtrace_cleanup(void) {
    // Nothing to clean up at this point
}

bool backtrace_capture(pid_t pid, thread_backtrace_t* backtrace) {
    if (backtrace == NULL) {
        return false;
    }
    
    // Initialize backtrace structure
    memset(backtrace, 0, sizeof(thread_backtrace_t));
    backtrace->thread_id = pid;
    
    // Get the executable path if we don't have it yet
    if (executable_path[0] == '\0') {
        if (!get_process_executable(pid, executable_path, sizeof(executable_path))) {
            debug_print("Failed to get executable path for process %d\n", pid);
        } else {
            debug_print("Executable path: %s\n", executable_path);
        }
    }
    
    // Capture stack frames
    void* frames[MAX_BACKTRACE_DEPTH] = {0};
    int frame_count = unwind_stack_fp(pid, frames, MAX_BACKTRACE_DEPTH);
    backtrace->frame_count = frame_count;
    
    // Process each frame
    for (int i = 0; i < frame_count; i++) {
        backtrace->frames[i].address = frames[i];
        
        // Get symbol information if available
        symbol_info_t symbol;
        if (find_symbol_info(frames[i], &symbol)) {
            strncpy(backtrace->frames[i].symbol_name, symbol.name, MAX_SYMBOL_LENGTH - 1);
            backtrace->frames[i].symbol_addr = symbol.addr;
            backtrace->frames[i].offset = (unsigned long)frames[i] - (unsigned long)symbol.addr;
        } else {
            snprintf(backtrace->frames[i].symbol_name, MAX_SYMBOL_LENGTH, "??");
            backtrace->frames[i].symbol_addr = NULL;
            backtrace->frames[i].offset = 0;
        }
    }
    
    return frame_count > 0;
}

bool backtrace_capture_all_threads(pid_t pid, thread_backtrace_t* backtraces,
                                 size_t max_threads, size_t* thread_count) {
    char task_path[64];
    snprintf(task_path, sizeof(task_path), "/proc/%d/task", pid);
    
    DIR* dir = opendir(task_path);
    if (!dir) {
        perror("opendir");
        return false;
    }
    
    // Collect all thread IDs
    struct dirent* entry;
    pid_t thread_ids[max_threads];
    size_t count = 0;
    
    while ((entry = readdir(dir)) != NULL && count < max_threads) {
        if (entry->d_name[0] != '.') {
            thread_ids[count++] = atoi(entry->d_name);
        }
    }
    
    closedir(dir);
    *thread_count = count;
    
    // Capture backtrace for each thread
    size_t success_count = 0;
    for (size_t i = 0; i < count; i++) {
        if (backtrace_capture(thread_ids[i], &backtraces[i])) {
            success_count++;
        }
    }
    
    return success_count > 0;
}

bool backtrace_is_waiting_for_mutex(const thread_backtrace_t* backtrace, void** mutex_addr) {
    if (backtrace == NULL || backtrace->frame_count == 0) {
        return false;
    }
    
    // Look for pthread_mutex_lock in the backtrace
    for (int i = 0; i < backtrace->frame_count; i++) {
        const char* name = backtrace->frames[i].symbol_name;
        
        // Check for functions that indicate waiting for a mutex
        if (strstr(name, "pthread_mutex_lock") != NULL ||
            strstr(name, "__lll_lock_wait") != NULL ||
            strstr(name, "futex_wait") != NULL) {
            
            // In a real implementation, we would extract the mutex address from the
            // function arguments or from memory near the stack/registers.
            // For this simplified version, we just indicate that a mutex wait was found.
            
            if (mutex_addr != NULL) {
                // This is a placeholder. In a real implementation, we would:
                // 1. Get the function arguments using register values or stack memory
                // 2. Extract the mutex address from the appropriate argument
                *mutex_addr = NULL; // Not implemented yet
            }
            
            return true;
        }
    }
    
    return false;
}

bool backtrace_detect_deadlocks(thread_backtrace_t* backtraces, size_t thread_count) {
    if (backtraces == NULL || thread_count == 0) {
        return false;
    }
    
    // Count threads waiting for mutexes
    int waiting_threads = 0;
    for (size_t i = 0; i < thread_count; i++) {
        void* mutex_addr;
        if (backtrace_is_waiting_for_mutex(&backtraces[i], &mutex_addr)) {
            waiting_threads++;
        }
    }
    
    // If multiple threads are waiting, it could be a deadlock
    // This is a very simplified detection - real detection would:
    // 1. Build a wait-for graph based on which thread owns which mutex
    // 2. Check for cycles in this graph
    
    if (waiting_threads >= 2) {
        debug_print("Potential deadlock detected: %d threads waiting for mutexes\n", waiting_threads);
        
        // Print details for all waiting threads
        for (size_t i = 0; i < thread_count; i++) {
            void* mutex_addr;
            if (backtrace_is_waiting_for_mutex(&backtraces[i], &mutex_addr)) {
                fprintf(stderr, "Thread %d waiting for a mutex. Stack trace:\n", backtraces[i].thread_id);
                backtrace_print(&backtraces[i]);
            }
        }
        
        return true;
    }
    
    return false;
}

void backtrace_print(const thread_backtrace_t* backtrace) {
    if (backtrace == NULL) {
        return;
    }
    
    printf("Thread %d backtrace (%d frames):\n", backtrace->thread_id, backtrace->frame_count);
    
    for (int i = 0; i < backtrace->frame_count; i++) {
        const stack_frame_t* frame = &backtrace->frames[i];
        
        if (frame->offset > 0) {
            printf("#%d: %p %s+%lx\n", i, frame->address, frame->symbol_name, frame->offset);
        } else {
            printf("#%d: %p %s\n", i, frame->address, frame->symbol_name);
        }
    }
}

void backtrace_set_use_debug_symbols(bool enable) {
    use_debug_symbols = enable;
}