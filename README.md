# lockdep

A lockdep (lock dependency) implementation in C that provides deadlock detection capabilities for any pthread-based program, inspired by the Linux kernel's lockdep system. This implementation detects both actual deadlocks through cycle detection in a wait-for graph and potential deadlocks by detecting lock acquisition order violations.

## Quick Start

- **Compilation**

    First, setup the `build` directory with:

    ```bash
    cmake -S . -B build
    ```

    Then, you can compile the project with:

    ```bash
    cmake --build build
    ```

- **Usage:**

    Once the build is complete, you can use the lockdep library by preloading it with your pthread-based program. For example:

    ```bash
    # Run simple lock ordering test
    LD_PRELOAD=./build/liblockdep_interpose.so ./build/simple_test

    # Run classic AB-BA deadlock test
    LD_PRELOAD=./build/liblockdep_interpose.so ./build/deadlock_test

    # Test with any existing pthread program
    LD_PRELOAD=./build/liblockdep_interpose.so /path/to/your/program
    ```

## Implementation Details

### Architecture

The lockdep system consists of three main components:

1. **Core Lockdep Engine** - Manages the lock dependency graph and performs cycle detection
2. **Function Interposition Layer** - Intercepts pthread mutex calls to monitor lock acquisition/release
3. **API Interface** - Provides functions for deadlock detection and dependency tracking

### Features

- **Lock Dependency Tracking**: Records the order in which locks are acquired
- **Cycle Detection**: Uses depth-first search to find cycles in the dependency graph
- **Order Violation Detection**: Warns when locks are acquired in an inconsistent order
- **Runtime Disabling**: Can be disabled at runtime using environment variables
- **Detailed Diagnostics**: Provides backtraces when lock order violations are detected

### Key Data Structures

- `lock_node_t`: Represents a lock in the dependency graph
- `dependency_edge_t`: Represents a directed edge (A→B) in the graph
- `thread_context_t`: Tracks locks held by each thread
- `held_lock_t`: Stack of locks currently held by a thread

### Algorithm

The deadlock detection works as follows:

1. When a thread acquires a lock, we check if it holds other locks
2. If it holds other locks, we establish a dependency: previously_acquired → newly_acquired
3. We check if this new dependency creates a cycle in the global dependency graph
4. If a cycle is detected, we warn about a potential deadlock

The system can detect two types of issues:
- **Lock Order Violation**: When different threads acquire the same locks in different orders
- **Actual Deadlock**: When a cycle exists in the global lock dependency graph
    ```

- **Environment variables:**

    You can disable the lockdep system during runtime by setting the `LOCKDEP_DISABLE` environment variable to `1`. For example:

    ```bash
    LOCKDEP_DISABLE=1 LD_PRELOAD=./build/liblockdep_interpose.so ./your_program
    ```

## Examples

### Example 1: Simple Lock Ordering

The `simple_test.c` demonstrates establishing a lock ordering without deadlock:

```c
pthread_mutex_lock(&mutex1);
pthread_mutex_lock(&mutex2);
// Both locks acquired successfully
pthread_mutex_unlock(&mutex2);
pthread_mutex_unlock(&mutex1);
```

### Example 2: Deadlock Scenario (AB-BA Problem)

The `deadlock_test.c` demonstrates a classic deadlock scenario:

```c
// Thread 1                   | Thread 2
pthread_mutex_lock(&mutex1);  | pthread_mutex_lock(&mutex2);
// Thread 1 waits for mutex2  | // Thread 2 waits for mutex1
pthread_mutex_lock(&mutex2);  | pthread_mutex_lock(&mutex1);
```

When run with lockdep, this will be detected before an actual deadlock occurs!

## Limitations

- Only detects deadlocks involving pthread mutexes
- Does not detect deadlocks involving other synchronization primitives
- May introduce some runtime overhead

## CONTRIBUTING

### Code Formatting

Make sure to align with the project's code style. The project uses `clang-format` for code formatting. You can check if your code is formatted correctly by running:

```bash
clang-format --dry-run --Werror src/*.c src/include/*.h
```

You can also format everything by running this:

```bash
find . -name "*.c" -o -name "*.h" | xargs clang-format -i
```

### Git

- Refer to [TODO.md](./TODO.md) to see what you can do. (We may improve this model later on to a issue-kanban-like system)
- When developing a new functionality, create a new branch from `main` with a descriptive name, for example `feat/lockdep-debug-system`. When the functionality is ready, create a pull request to `main` with a description of the changes made.
- Preferably, rebase your branch with the latest changes from `main` before creating the pull request.
- Make sure to make use of [conventional commits standard](https://www.conventionalcommits.org/en/v1.0.0/).
