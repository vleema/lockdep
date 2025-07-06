# ptrace-based Deadlock Detection

This directory contains an implementation of deadlock detection using the ptrace API, which allows monitoring and analyzing lock acquisition patterns without modifying the target program or using LD_PRELOAD.

## Overview

Unlike the interposition-based approach, ptrace-based deadlock detection works by:

1. Attaching to a running process using the ptrace API
2. Intercepting relevant system calls related to mutex operations
3. Analyzing the process's memory to extract information about locks and threads
4. Building a lock dependency graph externally
5. Detecting potential deadlocks without modifying the target program

## Components

The implementation consists of several components:

- **ptrace_attach.c**: Handles attaching to and detaching from target processes
- **syscall_intercept.c**: Intercepts and processes relevant system calls
- **memory_access.c**: Provides functions for accessing the memory of the target process
- **pthread_structures.c**: Interprets pthread mutex structures in the target process's memory
- **lock_tracker.c**: Tracks lock acquisitions and releases
- **detector.c**: Analyzes the lock graph to detect potential deadlocks
- **main.c**: Command-line interface for the ptrace-based detector

## Usage

```
ptrace-lockdep [OPTIONS] PID

OPTIONS:
  -h, --help            Show this help message
  -v, --verbose         Enable verbose output
  -a, --all-threads     Monitor all threads (default: only main thread)
  -t, --timeout=SECS    Set monitoring timeout in seconds (default: run until Ctrl+C)
  -d, --detect-only     Only detect deadlocks, don't modify process behavior
  -i, --interval=SECS   Analysis interval in seconds (default: 1)
```

## Implementation Details

### System Call Interception

The implementation intercepts the following system calls:

- `futex`: Used by pthread mutexes for lock operations
- `clone`/`fork`: To track thread creation
- `exit`: To track thread termination

### Memory Access

To understand the state of mutexes, we need to:

1. Locate pthread mutex structures in the target process's memory
2. Read the mutex state (locked/unlocked, owner thread)
3. Build a mapping between mutex addresses and their current state

### Lock Dependency Tracking

Similar to the LD_PRELOAD approach, we build a directed graph where:

- Nodes represent mutexes in the target process
- Edges represent the order of mutex acquisition

The main difference is that all analysis happens externally without modifying the target process's behavior.

### Limitations

Ptrace-based monitoring has several limitations:

1. **Performance Impact**: Ptrace causes significant slowdown due to intercepting each system call
2. **Implementation Complexity**: Interpreting pthread structures requires deep understanding of glibc internals
3. **Glibc Version Dependence**: Changes in glibc's internal structures may break the detector
4. **Limited Visibility**: Some lock operations might not use system calls directly
5. **Race Conditions**: May miss events due to the asynchronous nature of monitoring

### Advantages

Despite limitations, ptrace-based monitoring offers unique advantages:

1. **No Modification Required**: Works with unmodified binaries
2. **Runtime Attachment**: Can attach to already running processes
3. **Works with Static Linking**: Can monitor statically linked executables
4. **Non-Invasive Analysis**: Doesn't affect the binary's behavior beyond the performance impact
5. **Deep Introspection**: Can examine additional process state beyond just mutex operations

## Future Work

- Support for additional synchronization primitives (semaphores, rwlocks, etc.)
- Improved performance through selective system call filtering
- Integration with the graphical interface for real-time monitoring
- Support for attaching to containerized processes