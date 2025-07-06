/**
 * @file lock_tracker.h
 * @brief Interface for tracking locks and detecting deadlocks using ptrace
 *
 * This file provides the interface for a lock tracking system that monitors
 * mutex operations in a traced process, builds a lock dependency graph,
 * and detects potential deadlocks.
 */

#ifndef LOCK_TRACKER_H
#define LOCK_TRACKER_H

#include <sys/types.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Initialize the lock tracker
 * 
 * This function initializes the lock tracking system, creating necessary
 * data structures for tracking locks and dependencies.
 * 
 * @return true if initialization was successful, false otherwise
 */
bool lock_tracker_init(void);

/**
 * @brief Clean up resources used by the lock tracker
 */
void lock_tracker_cleanup(void);

/**
 * @brief Register a new thread with the lock tracker
 * 
 * This function registers a new thread with the lock tracking system.
 * 
 * @param thread_id The ID of the thread to register
 * @return true if registration was successful, false otherwise
 */
bool lock_tracker_register_thread(pid_t thread_id);

/**
 * @brief Unregister a thread from the lock tracker
 * 
 * This function unregisters a thread from the lock tracking system,
 * cleaning up any associated resources.
 * 
 * @param thread_id The ID of the thread to unregister
 */
void lock_tracker_unregister_thread(pid_t thread_id);

/**
 * @brief Register a lock acquisition
 * 
 * This function notifies the tracker that a thread has acquired a lock.
 * 
 * @param thread_id The ID of the thread acquiring the lock
 * @param lock_addr The address of the acquired lock
 * @param is_recursive Whether this is a recursive acquisition of the lock
 * @return true if registration was successful, false if a potential deadlock is detected
 */
bool lock_tracker_register_acquisition(pid_t thread_id, void* lock_addr, bool is_recursive);

/**
 * @brief Register a lock release
 * 
 * This function notifies the tracker that a thread has released a lock.
 * 
 * @param thread_id The ID of the thread releasing the lock
 * @param lock_addr The address of the released lock
 */
void lock_tracker_register_release(pid_t thread_id, void* lock_addr);

/**
 * @brief Check for potential deadlocks
 * 
 * This function analyzes the current lock graph to detect potential deadlocks.
 * 
 * @return true if a potential deadlock is detected, false otherwise
 */
bool lock_tracker_check_deadlocks(void);

/**
 * @brief Print the current lock dependency graph
 * 
 * This function prints a representation of the current lock dependency graph.
 */
void lock_tracker_print_graph(void);

/**
 * @brief Print information about locks held by all threads
 * 
 * This function prints information about which locks are currently held by each thread.
 */
void lock_tracker_print_thread_locks(void);

/**
 * @brief Get the number of locks currently being tracked
 * 
 * @return The number of locks
 */
size_t lock_tracker_get_lock_count(void);

/**
 * @brief Get the number of threads currently being tracked
 * 
 * @return The number of threads
 */
size_t lock_tracker_get_thread_count(void);

/**
 * @brief Enable or disable verbose output
 * 
 * @param enable Whether to enable verbose output
 */
void lock_tracker_set_verbose(bool enable);

#endif /* LOCK_TRACKER_H */