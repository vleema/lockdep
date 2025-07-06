#!/bin/bash

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Get the directory of this script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"

# Create build directory if it doesn't exist
mkdir -p "$BUILD_DIR"

# Function to display usage
usage() {
    echo -e "${BLUE}Lockdep Test Runner${NC}"
    echo -e "Usage: $0 [options] [test_name]"
    echo
    echo "Options:"
    echo "  -b, --build           Build the project before running tests"
    echo "  -i, --interpose       Use LD_PRELOAD interposition (default)"
    echo "  -p, --ptrace          Use ptrace-based monitoring"
    echo "  -a, --all             Run all tests"
    echo "  -v, --verbose         Show verbose output"
    echo "  -h, --help            Show this help message"
    echo
    echo -e "If no test name is provided, all tests will be run."
    echo -e "Available tests (in didactic order):"

    # List all test files in numeric order
    for test_file in "$SCRIPT_DIR"/t*.c; do
        test_name=$(basename "$test_file" .c)
        echo "  - $test_name"
    done
}

# Parse command line arguments
BUILD=0
USE_PTRACE=0
RUN_ALL=0
VERBOSE=0
SPECIFIC_TEST=""

while [[ $# -gt 0 ]]; do
    case $1 in
        -b|--build)
            BUILD=1
            shift
            ;;
        -i|--interpose)
            USE_PTRACE=0
            shift
            ;;
        -p|--ptrace)
            USE_PTRACE=1
            shift
            ;;
        -a|--all)
            RUN_ALL=1
            shift
            ;;
        -v|--verbose)
            VERBOSE=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            # Assume this is a test name
            SPECIFIC_TEST="$1"
            shift
            ;;
    esac
done

# Build the project if requested
if [ $BUILD -eq 1 ]; then
    echo -e "${BLUE}Building project...${NC}"
    cd "$BUILD_DIR" || { echo -e "${RED}Failed to change to build directory${NC}"; exit 1; }
    cmake .. && make
    if [ $? -ne 0 ]; then
        echo -e "${RED}Build failed${NC}"
        exit 1
    fi
    cd - > /dev/null || { echo -e "${RED}Failed to return to original directory${NC}"; exit 1; }
fi

# Check if build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${RED}Build directory not found. Run with --build or build the project first.${NC}"
    exit 1
fi

# Function to run a single test
run_test() {
    local test_name="$1"
    local test_executable="$BUILD_DIR/$test_name"

    # Check if executable exists
    if [ ! -x "$test_executable" ]; then
        echo -e "${RED}Test executable '$test_name' not found. Did you build the project?${NC}"
        return 1
    fi

    echo -e ""
    echo -e "${YELLOW}======================================${NC}"
    echo -e "${YELLOW}Running test: $test_name${NC}"
    echo -e "${YELLOW}======================================${NC}"

    if [ $USE_PTRACE -eq 1 ]; then
        # Use ptrace-based monitoring
        echo -e "${BLUE}Using ptrace-based monitoring${NC}"

        # Run the test in background
        "$test_executable" &
        TEST_PID=$!

        # Give it a moment to start
        sleep 0.5

        # Then attach with ptrace
        if [ $VERBOSE -eq 1 ]; then
            "$BUILD_DIR/ptrace-lockdep" --all-threads --verbose "$TEST_PID"
        else
            "$BUILD_DIR/ptrace-lockdep" --all-threads "$TEST_PID"
        fi

        # Wait for the test to complete
        wait $TEST_PID
    else
        # Use LD_PRELOAD interposition
        echo -e "${BLUE}Using LD_PRELOAD interposition${NC}"
        LD_PRELOAD="$BUILD_DIR/liblockdep_interpose.so" "$test_executable"
    fi

    local result=$?
    if [ $result -eq 0 ]; then
        echo -e "${GREEN}Test '$test_name' completed successfully${NC}"
    else
        echo -e "${RED}Test '$test_name' failed with exit code $result${NC}"
    fi

    return $result
}

# Get list of all tests
get_all_tests() {
    local tests=()
    # Sort tests by their numeric prefix to maintain didactic order
    for test_file in $(ls "$SCRIPT_DIR"/t*.c | sort); do
        test_name=$(basename "$test_file" .c)
        tests+=("$test_name")
    done
    echo "${tests[@]}"
}

# Run tests
if [ -n "$SPECIFIC_TEST" ]; then
    # Run specific test
    run_test "$SPECIFIC_TEST"
    exit $?
else
    # Run all tests
    FAILED_TESTS=()
    PASSED_TESTS=()

    for test_name in $(get_all_tests); do
        run_test "$test_name"
        if [ $? -eq 0 ]; then
            PASSED_TESTS+=("$test_name")
        else
            FAILED_TESTS+=("$test_name")
        fi
    done

    # Print summary
    echo -e "\n${YELLOW}======================================${NC}"
    echo -e "${YELLOW}Test Summary${NC}"
    echo -e "${YELLOW}======================================${NC}"
    echo -e "${GREEN}Passed: ${#PASSED_TESTS[@]}${NC}"
    echo -e "${RED}Failed: ${#FAILED_TESTS[@]}${NC}"

    echo -e "\n${CYAN}Didactic Test Progression:${NC}"
    echo -e "${CYAN}------------------------${NC}"
    echo -e "${CYAN}t01-t02: Basic lock ordering and classic deadlock${NC}"
    echo -e "${CYAN}t03-t06: Advanced deadlock scenarios${NC}"
    echo -e "${CYAN}t07-t09: Deadlock avoidance techniques${NC}"

    if [ ${#FAILED_TESTS[@]} -gt 0 ]; then
        echo -e "\n${RED}Failed tests:${NC}"
        for test in "${FAILED_TESTS[@]}"; do
            echo -e "${RED}- $test${NC}"
        done
        exit 1
    fi

    exit 0
fi
