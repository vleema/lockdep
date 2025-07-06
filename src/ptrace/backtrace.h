/**
 * @file backtrace.h
 * @brief Functions for analyzing backtraces of threads in a traced process
 *
 * This file provides the interface for extracting and analyzing stack traces
 * from threads in a traced process, helping to identify where threads are
 * blocked in potential deadlock situations.
 */

#ifndef BACKTRACE_H
#define BACKTRACE_H

#include <sys/types.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Maximum depth of stack frames to capture
 */
#define MAX_BACKTRACE_DEPTH 128

/**
 * @brief Maximum length of a symbol name
 */
#define MAX_SYMBOL_LENGTH 256

/**
 * @brief Information about a stack frame
 */
typedef struct {
    void* address;                    // Instruction pointer address
    char symbol_name[MAX_SYMBOL_LENGTH]; // Symbol name if available
    void* symbol_addr;                // Start address of the symbol
    unsigned long offset;             // Offset from symbol start
} stack_frame_t;

/**
 * @brief Complete backtrace information for a thread
 */
typedef struct {
    pid_t thread_id;                  // ID of the thread
    int frame_count;                  // Number of frames in the trace
    stack_frame_t frames[MAX_BACKTRACE_DEPTH]; // The stack frames
} thread_backtrace_t;

/**
 * @brief Initialize the backtrace analysis subsystem
 * 
 * Sets up any necessary resources for stack trace analysis.
 * 
 * @return true if initialization was successful, false otherwise
 */
bool backtrace_init(void);

/**
 * @brief Clean up resources used by the backtrace analysis
 */
void backtrace_cleanup(void);

/**
 * @brief Capture a backtrace from a thread in the traced process
 * 
 * This function captures a stack trace from the specified thread,
 * including symbol resolution if possible.
 * 
 * @param pid The process/thread ID to capture from
 * @param backtrace Pointer to structure that will receive the backtrace
 * @return true if the backtrace was successfully captured, false otherwise
 */
bool backtrace_capture(pid_t pid, thread_backtrace_t* backtrace);

/**
 * @brief Capture backtraces from all threads in the traced process
 * 
 * This function captures stack traces from all threads in the process.
 * 
 * @param pid The main process ID
 * @param backtraces Array to receive the backtraces
 * @param max_threads Maximum number of threads to capture
 * @param thread_count Pointer to variable that will receive the number of threads captured
 * @return true if the backtraces were successfully captured, false otherwise
 */
bool backtrace_capture_all_threads(pid_t pid, thread_backtrace_t* backtraces,
                                 size_t max_threads, size_t* thread_count);

/**
 * @brief Check if a backtrace indicates the thread is waiting for a mutex
 * 
 * This function analyzes a backtrace to determine if the thread appears
 * to be blocked waiting for a mutex.
 * 
 * @param backtrace The backtrace to analyze
 * @param mutex_addr Pointer to variable that will receive the mutex address if found
 * @return true if the thread appears to be waiting for a mutex, false otherwise
 */
bool backtrace_is_waiting_for_mutex(const thread_backtrace_t* backtrace, void** mutex_addr);

/**
 * @brief Detect deadlocks by analyzing thread backtraces
 * 
 * This function analyzes backtraces from multiple threads to identify
 * potential deadlock situations.
 * 
 * @param backtraces Array of thread backtraces
 * @param thread_count Number of threads in the array
 * @return true if a potential deadlock was detected, false otherwise
 */
bool backtrace_detect_deadlocks(thread_backtrace_t* backtraces, size_t thread_count);

/**
 * @brief Print a human-readable representation of a backtrace
 * 
 * @param backtrace The backtrace to print
 */
void backtrace_print(const thread_backtrace_t* backtrace);

/**
 * @brief Enable or disable loading of debug symbols
 * 
 * If debug symbols are available for the target process, they can be used
 * to get more accurate backtraces with symbol names.
 * 
 * @param enable Whether to enable debug symbol loading
 */
void backtrace_set_use_debug_symbols(bool enable);

#endif /* BACKTRACE_H */