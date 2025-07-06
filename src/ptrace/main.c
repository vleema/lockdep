/**
 * @file main.c
 * @brief Main program for ptrace-based deadlock detection
 *
 * This file implements a command-line utility that uses ptrace to monitor
 * a target process, track its mutex operations, and detect potential deadlocks.
 */

#include "ptrace_attach.h"
#include "syscall_intercept.h"
#include "pthread_structures.h"
#include "lock_tracker.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

// Global variables
static volatile sig_atomic_t exit_requested = 0;
static pid_t target_pid = -1;
static int timeout_seconds = 0;
static bool monitor_all_threads = false;
static bool detect_only = false;
static int analysis_interval = 1;
static bool verbose = false;

// Signal handler for graceful termination
static void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        fprintf(stderr, "Termination signal received, cleaning up...\n");
        exit_requested = 1;
    }
}

// Print usage information
static void print_usage(const char* program_name) {
    printf("Usage: %s [OPTIONS] PID\n\n", program_name);
    printf("Options:\n");
    printf("  -h, --help            Show this help message\n");
    printf("  -v, --verbose         Enable verbose output\n");
    printf("  -a, --all-threads     Monitor all threads (default: only main thread)\n");
    printf("  -t, --timeout=SECS    Set monitoring timeout in seconds (default: run until Ctrl+C)\n");
    printf("  -d, --detect-only     Only detect deadlocks, don't modify process behavior\n");
    printf("  -i, --interval=SECS   Analysis interval in seconds (default: 1)\n");
}

// Parse command line arguments
static bool parse_arguments(int argc, char* argv[]) {
    struct option long_options[] = {
        {"help", no_argument, NULL, 'h'},
        {"verbose", no_argument, NULL, 'v'},
        {"all-threads", no_argument, NULL, 'a'},
        {"timeout", required_argument, NULL, 't'},
        {"detect-only", no_argument, NULL, 'd'},
        {"interval", required_argument, NULL, 'i'},
        {NULL, 0, NULL, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "hvat:di:", long_options, NULL)) != -1) {
        switch (opt) {
            case 'h':
                print_usage(argv[0]);
                exit(EXIT_SUCCESS);
            case 'v':
                verbose = true;
                break;
            case 'a':
                monitor_all_threads = true;
                break;
            case 't':
                timeout_seconds = atoi(optarg);
                if (timeout_seconds <= 0) {
                    fprintf(stderr, "Invalid timeout value: %s\n", optarg);
                    return false;
                }
                break;
            case 'd':
                detect_only = true;
                break;
            case 'i':
                analysis_interval = atoi(optarg);
                if (analysis_interval <= 0) {
                    fprintf(stderr, "Invalid interval value: %s\n", optarg);
                    return false;
                }
                break;
            default:
                print_usage(argv[0]);
                return false;
        }
    }

    // Check if we have a PID argument
    if (optind >= argc) {
        fprintf(stderr, "Missing PID argument\n");
        print_usage(argv[0]);
        return false;
    }

    // Parse PID
    target_pid = atoi(argv[optind]);
    if (target_pid <= 0) {
        fprintf(stderr, "Invalid PID: %s\n", argv[optind]);
        return false;
    }

    return true;
}

// Initialize subsystems
static bool initialize_subsystems() {
    // Set verbosity for all components
    syscall_set_verbose(verbose);
    lock_tracker_set_verbose(verbose);

    // Initialize lock tracker
    if (!lock_tracker_init()) {
        fprintf(stderr, "Failed to initialize lock tracker\n");
        return false;
    }

    // Initialize syscall interception
    if (!syscall_intercept_init()) {
        fprintf(stderr, "Failed to initialize syscall interception\n");
        lock_tracker_cleanup();
        return false;
    }

    // Initialize pthread structures
    if (!pthread_structures_init()) {
        fprintf(stderr, "Failed to initialize pthread structures\n");
        syscall_intercept_cleanup();
        lock_tracker_cleanup();
        return false;
    }

    return true;
}

// Clean up subsystems
static void cleanup_subsystems() {
    pthread_structures_cleanup();
    syscall_intercept_cleanup();
    lock_tracker_cleanup();
}

// Detach from the traced process
static void detach_from_process() {
    if (target_pid > 0) {
        ptrace_detach(target_pid);
    }
}

// Periodically analyze the lock graph for deadlocks
static void *analysis_thread_func(void *arg) {
    (void)arg; // Unused

    while (!exit_requested) {
        // Sleep for the specified interval
        sleep(analysis_interval);
        
        // Check for deadlocks
        if (lock_tracker_check_deadlocks()) {
            fprintf(stderr, "Deadlock detected! Lock graph state:\n");
            lock_tracker_print_graph();
            lock_tracker_print_thread_locks();
            
            // If we're in detect-only mode, continue running
            if (!detect_only) {
                fprintf(stderr, "Exiting due to deadlock detection\n");
                exit_requested = 1;
                break;
            }
        }
    }
    
    return NULL;
}

// Main tracing loop
static void trace_process() {
    // Register the main thread with the lock tracker
    lock_tracker_register_thread(target_pid);
    
    // Create a pthread to periodically analyze the lock graph
    pthread_t analysis_thread;
    pthread_create(&analysis_thread, NULL, analysis_thread_func, NULL);
    
    // Start time for timeout
    time_t start_time = time(NULL);
    
    // Main tracing loop
    while (!exit_requested) {
        // Check timeout if specified
        if (timeout_seconds > 0 && difftime(time(NULL), start_time) >= timeout_seconds) {
            fprintf(stderr, "Timeout reached (%d seconds)\n", timeout_seconds);
            break;
        }
        
        // Wait for the next system call
        bool entering;
        long syscall = ptrace_wait_for_syscall(target_pid, &entering);
        
        if (syscall == -1) {
            // Process likely exited
            fprintf(stderr, "Lost connection to process %d\n", target_pid);
            break;
        }
        
        // Process the system call
        syscall_process(target_pid, syscall, entering);
    }
    
    // Wait for the analysis thread to finish
    pthread_cancel(analysis_thread);
    pthread_join(analysis_thread, NULL);
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    if (!parse_arguments(argc, argv)) {
        return EXIT_FAILURE;
    }
    
    printf("Monitoring process %d for deadlocks\n", target_pid);
    if (monitor_all_threads) {
        printf("Monitoring all threads\n");
    }
    if (timeout_seconds > 0) {
        printf("Will monitor for %d seconds\n", timeout_seconds);
    }
    
    // Set up signal handlers for graceful termination
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    
    // Initialize subsystems
    if (!initialize_subsystems()) {
        fprintf(stderr, "Initialization failed\n");
        return EXIT_FAILURE;
    }
    
    // Attach to the target process
    if (!ptrace_attach(target_pid)) {
        fprintf(stderr, "Failed to attach to process %d\n", target_pid);
        cleanup_subsystems();
        return EXIT_FAILURE;
    }
    
    // If requested, attach to all threads
    if (monitor_all_threads) {
        int attached = ptrace_attach_all_threads(target_pid);
        if (attached == -1) {
            fprintf(stderr, "Failed to attach to threads\n");
            ptrace_detach(target_pid);
            cleanup_subsystems();
            return EXIT_FAILURE;
        }
        printf("Attached to %d threads\n", attached);
    }
    
    // Perform the tracing
    trace_process();
    
    // Clean up
    detach_from_process();
    cleanup_subsystems();
    
    printf("Monitoring complete\n");
    
    return EXIT_SUCCESS;
}