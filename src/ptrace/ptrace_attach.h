/**
 * @file ptrace_attach.h
 * @brief Functions for attaching to and managing processes with ptrace
 *
 * This file provides the interface for attaching to processes using the
 * ptrace API for deadlock detection and monitoring lock acquisition patterns.
 */

#ifndef PTRACE_ATTACH_H
#define PTRACE_ATTACH_H

#include <sys/types.h>
#include <stdbool.h>

/**
 * @brief Attaches to a running process using ptrace
 * 
 * This function attaches to a running process with the specified PID,
 * enabling system call tracing.
 * 
 * @param pid The process ID to attach to
 * @return true if attachment was successful, false otherwise
 */
bool ptrace_attach(pid_t pid);

/**
 * @brief Detaches from a traced process
 * 
 * This function detaches from a previously attached process,
 * allowing it to continue execution normally.
 * 
 * @param pid The process ID to detach from
 * @return true if detachment was successful, false otherwise
 */
bool ptrace_detach(pid_t pid);

/**
 * @brief Attaches to all threads of a process
 * 
 * This function finds and attaches to all threads belonging to the
 * specified process.
 * 
 * @param pid The main process ID
 * @return Number of threads successfully attached to, or -1 on error
 */
int ptrace_attach_all_threads(pid_t pid);

/**
 * @brief Waits for the next system call in the traced process
 * 
 * This function sets up ptrace to stop the process at the next system call
 * entry or exit, and waits for this to happen.
 * 
 * @param pid The process ID to wait for
 * @param entering Pointer to a boolean that will be set to true if entering a syscall, 
 *                 false if exiting (can be NULL if not needed)
 * @return The system call number, or -1 on error
 */
long ptrace_wait_for_syscall(pid_t pid, bool* entering);

/**
 * @brief Gets the system call number for the current syscall
 * 
 * This function retrieves the system call number from the registers
 * of the traced process.
 * 
 * @param pid The process ID
 * @return The system call number, or -1 on error
 */
long ptrace_get_syscall_nr(pid_t pid);

/**
 * @brief Gets a system call argument
 * 
 * This function retrieves an argument for the current system call
 * from the registers of the traced process.
 * 
 * @param pid The process ID
 * @param arg_index The index of the argument (0-5)
 * @return The argument value as an unsigned long, or 0 on error
 */
unsigned long ptrace_get_syscall_arg(pid_t pid, int arg_index);

/**
 * @brief Gets the system call return value
 * 
 * This function retrieves the return value of a system call
 * from the registers of the traced process.
 * 
 * @param pid The process ID
 * @return The system call return value, or 0 on error
 */
long ptrace_get_syscall_result(pid_t pid);

/**
 * @brief Reads memory from the traced process
 * 
 * This function reads data from the memory space of the traced process.
 * 
 * @param pid The process ID
 * @param addr The address to read from
 * @param data Buffer to store the read data
 * @param len Length of data to read in bytes
 * @return true if read was successful, false otherwise
 */
bool ptrace_read_memory(pid_t pid, unsigned long addr, void* data, size_t len);

/**
 * @brief Writes memory to the traced process
 * 
 * This function writes data to the memory space of the traced process.
 * 
 * @param pid The process ID
 * @param addr The address to write to
 * @param data Buffer containing the data to write
 * @param len Length of data to write in bytes
 * @return true if write was successful, false otherwise
 */
bool ptrace_write_memory(pid_t pid, unsigned long addr, const void* data, size_t len);

#endif /* PTRACE_ATTACH_H */