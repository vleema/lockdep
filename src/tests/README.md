# Lockdep Test Suite

This directory contains test programs designed to demonstrate various deadlock scenarios and lock acquisition patterns that can be detected by the lockdep tool.

## Running Tests

The test programs are automatically compiled by the CMake build system. To build and run tests:

```bash
# From the project root directory
mkdir -p build && cd build
cmake ..
make

# Run with lockdep (interposition method)
LD_PRELOAD=./liblockdep_interpose.so ./t01_simple_lock_order

# Or with ptrace-based monitoring
./ptrace-lockdep --all-threads ./t02_classic_deadlock
```

## Test Cases

### Basic Tests

1. **t01_simple_lock_order.c**
   - Creates a simple lock ordering pattern (mutex1 → mutex2) within a single thread
   - Demonstrates the most basic lock dependency tracking

2. **t02_classic_deadlock.c**
   - Classic AB-BA deadlock scenario with two threads
   - Thread 1 acquires mutex1, then tries to acquire mutex2
   - Thread 2 acquires mutex2, then tries to acquire mutex1
   - Demonstrates the classic deadlock pattern that lockdep should detect

### Advanced Deadlock Patterns

3. **t03_nested_deadlock.c**
   - Demonstrates a deadlock scenario with nested lock acquisitions
   - Thread 1 acquires mutex1, then mutex2, then tries to acquire mutex3
   - Thread 2 acquires mutex3, then tries to acquire mutex1
   - Tests lockdep's ability to detect deadlocks in complex nesting scenarios

4. **t04_circular_deadlock.c**
   - Creates a circular deadlock involving three threads
   - Thread 1: mutex1 → mutex2
   - Thread 2: mutex2 → mutex3
   - Thread 3: mutex3 → mutex1
   - Tests detection of deadlock cycles involving more than two threads

5. **t05_dining_philosophers.c**
   - Implements the classic dining philosophers problem
   - Five philosophers (threads) compete for five forks (mutexes)
   - Demonstrates how resource allocation patterns can lead to deadlock
   - Includes a solution that breaks the symmetry to avoid deadlock

6. **t06_dynamic_locks.c**
   - Tests deadlock detection with dynamically created locks
   - Includes array-based locks, locks created within threads, and locks in data structures
   - Verifies lockdep can track locks regardless of how they're created

### Deadlock Avoidance Techniques

7. **t07_recursive_locks.c**
   - Tests behavior of regular vs. recursive mutexes
   - Regular mutex will deadlock when locked recursively by the same thread
   - Recursive mutex allows the same thread to acquire it multiple times
   - Verifies lockdep's handling of recursive lock patterns

8. **t08_deadlock_avoidance_trylock.c**
   - Demonstrates how pthread_mutex_trylock can be used to avoid deadlocks
   - Two threads attempt to acquire the same set of locks in different orders
   - When trylock fails, threads release held locks and retry later
   - Shows a common deadlock avoidance strategy

9. **t09_deadlock_avoidance_timeout.c**
   - Uses pthread_mutex_timedlock to detect potential deadlocks
   - Two threads attempt to acquire the same set of locks in different orders
   - If acquisition times out, threads release held locks and retry
   - Demonstrates another deadlock avoidance technique

## Expected Results

When run with the lockdep system enabled, these tests should trigger lockdep warnings when potential deadlocks are detected. The warnings should occur before actual deadlocks happen, demonstrating lockdep's proactive detection capabilities.

Many of the tests are designed to not actually enter a deadlock state (which would hang the program). Instead, they create the conditions for lockdep to detect potential deadlock patterns.

## Test Progression

The tests are numbered in a didactic sequence:
- Tests 01-02: Basic lock ordering and classic deadlock pattern
- Tests 03-06: Advanced deadlock scenarios (nested locks, circular dependencies, classic concurrency problems, dynamic locks)
- Tests 07-09: Deadlock avoidance techniques (recursive locks, trylock, timeouts)

This sequence allows for a gradual understanding of deadlock issues and their solutions.

## Creating New Tests

When creating new test cases, consider the following pattern:

1. Document the lock acquisition pattern and expected behavior in comments
2. Create clear thread functions that demonstrate the pattern
3. Add print statements to show the progress through the critical sections
4. Consider using sleep() calls to make interleaving more predictable
5. If testing a deadlock avoidance technique, demonstrate both the problem and solution