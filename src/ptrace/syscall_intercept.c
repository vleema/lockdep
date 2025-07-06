/**
 * @file syscall_intercept.c
 * @brief Implementation of system call interception for mutex operations
 *
 * This file implements functions for intercepting and analyzing system calls,
 * particularly focusing on futex calls that are used by pthread mutexes.
 */

#include "syscall_intercept.h"
#include "ptrace_attach.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <linux/futex.h>
#include <sys/syscall.h>

// Maximum number of system calls we can handle
#define MAX_SYSCALL_NR 1024

// Verbose mode flag
static bool verbose = false;

// Array of syscall handlers
static syscall_handler_t syscall_handlers[MAX_SYSCALL_NR] = {0};

bool syscall_intercept_init(void) {
    // Register default handlers for the syscalls we're interested in
    syscall_register_handler(SYS_futex, syscall_handle_futex);
    syscall_register_handler(SYS_clone, syscall_handle_clone);
    syscall_register_handler(SYS_exit, syscall_handle_exit);
    syscall_register_handler(SYS_exit_group, syscall_handle_exit);
    
    return true;
}

void syscall_intercept_cleanup(void) {
    // Nothing to clean up at the moment
}

bool syscall_register_handler(long syscall_nr, syscall_handler_t handler) {
    if (syscall_nr < 0 || syscall_nr >= MAX_SYSCALL_NR) {
        fprintf(stderr, "Syscall number %ld out of range\n", syscall_nr);
        return false;
    }
    
    syscall_handlers[syscall_nr] = handler;
    return true;
}

bool syscall_process(pid_t pid, long syscall_nr, bool entering) {
    if (verbose) {
        const char* phase = entering ? "entering" : "exiting";
        fprintf(stderr, "Process %d %s syscall %ld\n", pid, phase, syscall_nr);
    }
    
    if (syscall_nr >= 0 && syscall_nr < MAX_SYSCALL_NR && syscall_handlers[syscall_nr] != NULL) {
        return syscall_handlers[syscall_nr](pid, entering);
    }
    
    return false; // Syscall not handled
}

bool syscall_handle_futex(pid_t pid, bool entering) {
    if (entering) {
        // When entering futex syscall, extract information about the operation
        unsigned long futex_uaddr = ptrace_get_syscall_arg(pid, 0);
        int futex_op = ptrace_get_syscall_arg(pid, 1);
        int futex_val = ptrace_get_syscall_arg(pid, 2);
        
        // Only process mutex lock/unlock operations
        int futex_cmd = futex_op & FUTEX_CMD_MASK;
        if (futex_cmd == FUTEX_WAIT || futex_cmd == FUTEX_WAKE) {
            if (verbose) {
                const char* op_name = (futex_cmd == FUTEX_WAIT) ? "WAIT" : "WAKE";
                fprintf(stderr, "Process %d futex %s: uaddr=%lx, val=%d\n",
                        pid, op_name, futex_uaddr, futex_val);
            }
            
            // Here we would interpret this as a mutex operation and update our lock graph
            // For FUTEX_WAIT, a thread is likely trying to acquire a lock
            // For FUTEX_WAKE, a thread is likely releasing a lock
            
            // Note: This is a simplified interpretation. In reality, pthread mutexes
            // use a complex sequence of operations that would need to be analyzed in context.
            
            return true;
        }
    } else {
        // When exiting futex syscall, check the result
        long result = ptrace_get_syscall_result(pid);
        
        if (verbose) {
            fprintf(stderr, "Process %d futex returned %ld\n", pid, result);
        }
        
        // Here we would update our lock graph based on the success/failure of the operation
    }
    
    return true;
}

bool syscall_handle_clone(pid_t pid, bool entering) {
    if (!entering) {
        // When exiting clone syscall, the return value is the new thread/process ID
        long new_pid = ptrace_get_syscall_result(pid);
        
        if (new_pid > 0) {
            if (verbose) {
                fprintf(stderr, "Process %d created new thread/process %ld\n", pid, new_pid);
            }
            
            // Here we would register the new thread in our tracking system
            // Note: We would need to determine if this is a thread or process based on the clone flags
        }
    }
    
    return true;
}

bool syscall_handle_exit(pid_t pid, bool entering) {
    if (entering) {
        // When a thread/process exits, clean up our data structures
        int exit_code = ptrace_get_syscall_arg(pid, 0);
        
        if (verbose) {
            fprintf(stderr, "Process %d exiting with code %d\n", pid, exit_code);
        }
        
        // Here we would clean up any tracking information for this thread
    }
    
    return true;
}

void syscall_set_verbose(bool enable) {
    verbose = enable;
}