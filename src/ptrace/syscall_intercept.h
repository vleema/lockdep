/**
 * @file syscall_intercept.h
 * @brief Functions for intercepting and analyzing system calls related to locking
 *
 * This file provides the interface for intercepting system calls made by the 
 * traced process, specifically those related to mutex operations (mainly futex calls).
 */

#ifndef SYSCALL_INTERCEPT_H
#define SYSCALL_INTERCEPT_H

#include <sys/types.h>
#include <stdbool.h>

/**
 * @brief System call handler function type
 * 
 * Function type for syscall handlers that process specific system calls.
 * 
 * @param pid The process ID of the traced process
 * @param entering True if entering the syscall, false if exiting
 * @return True if the syscall was handled, false otherwise
 */
typedef bool (*syscall_handler_t)(pid_t pid, bool entering);

/**
 * @brief Initialize syscall interception
 * 
 * Sets up the syscall interception subsystem and registers default handlers.
 * 
 * @return True if initialization was successful, false otherwise
 */
bool syscall_intercept_init(void);

/**
 * @brief Clean up resources used by syscall interception
 */
void syscall_intercept_cleanup(void);

/**
 * @brief Register a handler for a specific system call
 * 
 * @param syscall_nr The system call number to handle
 * @param handler The handler function to call when the syscall is intercepted
 * @return True if the handler was registered successfully, false otherwise
 */
bool syscall_register_handler(long syscall_nr, syscall_handler_t handler);

/**
 * @brief Process a system call
 * 
 * This function should be called when a system call is intercepted.
 * It will invoke the appropriate handler based on the syscall number.
 * 
 * @param pid The process ID of the traced process
 * @param syscall_nr The system call number
 * @param entering True if entering the syscall, false if exiting
 * @return True if the syscall was handled, false otherwise
 */
bool syscall_process(pid_t pid, long syscall_nr, bool entering);

/**
 * @brief Handle futex system call
 * 
 * Specifically processes futex system calls, which are used by pthread mutexes.
 * 
 * @param pid The process ID of the traced process
 * @param entering True if entering the syscall, false if exiting
 * @return True if the syscall was handled, false otherwise
 */
bool syscall_handle_futex(pid_t pid, bool entering);

/**
 * @brief Handle clone/fork system calls
 * 
 * Processes clone and fork system calls to track thread creation.
 * 
 * @param pid The process ID of the traced process
 * @param entering True if entering the syscall, false if exiting
 * @return True if the syscall was handled, false otherwise
 */
bool syscall_handle_clone(pid_t pid, bool entering);

/**
 * @brief Handle exit system call
 * 
 * Processes exit system calls to track thread termination.
 * 
 * @param pid The process ID of the traced process
 * @param entering True if entering the syscall, false if exiting
 * @return True if the syscall was handled, false otherwise
 */
bool syscall_handle_exit(pid_t pid, bool entering);

/**
 * @brief Enable verbose logging of syscall interception
 * 
 * @param enable True to enable verbose logging, false to disable
 */
void syscall_set_verbose(bool enable);

#endif /* SYSCALL_INTERCEPT_H */