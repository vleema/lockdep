# Graph Library for Deadlock Detection

This directory contains a reusable graph library that provides the core data structures and algorithms needed for deadlock detection. The library was designed to be independent of the specific lockdep implementation, allowing it to be reused in other contexts where directed graphs and cycle detection are needed.

## Features

- Directed graph representation using adjacency lists
- Operations for adding nodes and edges
- Cycle detection using depth-first search
- Support for arbitrary node identifiers (void pointers)
- Debug printing capabilities

## Core Data Structures

The library uses the following main data structures:

- `graph_t`: The main graph structure that maintains collections of nodes and edges
- `graph_node_t`: Represents a node in the graph, identified by an arbitrary pointer
- `graph_edge_t`: Represents a directed edge between two nodes

## API Overview

### Graph Management

- `graph_create()`: Creates a new empty graph
- `graph_destroy(graph)`: Frees all memory associated with the graph
- `graph_node_count(graph)`: Returns the number of nodes in the graph
- `graph_edge_count(graph)`: Returns the number of edges in the graph

### Node and Edge Operations

- `graph_find_or_create_node(graph, id)`: Gets or creates a node with the given identifier
- `graph_node_get_id(node)`: Returns the identifier of a node
- `graph_add_edge(graph, from, to)`: Adds a directed edge between two nodes
- `graph_get_all_nodes(graph, count)`: Returns an array of all nodes in the graph
- `graph_get_outgoing_edges(graph, node, count)`: Returns all nodes reachable from a given node

### Cycle Detection

- `graph_has_cycle(graph)`: Checks if the graph contains any cycles
- `graph_would_create_cycle(graph, from, to)`: Checks if adding an edge would create a cycle

### Debug Utilities

- `graph_print(graph, print_node_func)`: Prints a human-readable representation of the graph

## Usage Example

```c
#include "graph.h"
#include <stdio.h>

// Callback for printing node identifiers
const char* print_ptr(void* ptr) {
    static char buffer[32];
    snprintf(buffer, sizeof(buffer), "%p", ptr);
    return buffer;
}

int main() {
    // Create a graph
    graph_t* graph = graph_create();
    
    // Create some node identifiers (normally these would be mutex addresses)
    int node1_data = 1;
    int node2_data = 2;
    int node3_data = 3;
    
    // Add nodes and edges
    graph_node_t* n1 = graph_find_or_create_node(graph, &node1_data);
    graph_node_t* n2 = graph_find_or_create_node(graph, &node2_data);
    graph_node_t* n3 = graph_find_or_create_node(graph, &node3_data);
    
    graph_add_edge(graph, n1, n2);
    graph_add_edge(graph, n2, n3);
    
    // Check if adding an edge would create a cycle
    if (graph_would_create_cycle(graph, n3, n1)) {
        printf("Adding edge would create a cycle!\n");
    }
    
    // Print the graph
    graph_print(graph, print_ptr);
    
    // Clean up
    graph_destroy(graph);
    
    return 0;
}
```

## Integration with Lockdep

This graph library is used by the lockdep system to:

1. Represent locks as nodes in the graph
2. Track the order of lock acquisitions as directed edges
3. Detect potential deadlocks by checking for cycles in the graph
4. Prevent deadlocks by checking if new lock acquisitions would create cycles

By separating the graph implementation from the lockdep-specific code, we achieve:

- Better code organization and modularity
- Improved testability of both components
- The ability to reuse the graph library for other purposes
- Cleaner maintenance and future enhancements