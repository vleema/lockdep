/**
 * @file graph.c
 * @brief Implementation of graph library for deadlock detection
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "graph.h"

/**
 * @brief Node structure for the graph
 */
struct graph_node {
    void* id;                // Unique identifier for the node
    size_t index;            // Index in the graph's node array (for algorithms)
    struct graph_node* next; // For traversal in the node list
};

/**
 * @brief Edge structure for the graph
 */
struct graph_edge {
    graph_node_t* from;       // Source node
    graph_node_t* to;         // Destination node
    struct graph_edge* next;  // For traversal in the edge list
};

/**
 * @brief Graph structure
 */
struct graph {
    graph_node_t* nodes;      // Linked list of all nodes
    graph_edge_t* edges;      // Linked list of all edges
    size_t node_count;        // Number of nodes in the graph
    size_t edge_count;        // Number of edges in the graph
};

/**
 * Creates a new directed graph
 */
graph_t* graph_create(void) {
    graph_t* graph = malloc(sizeof(graph_t));
    if (graph == NULL) {
        return NULL;
    }
    
    graph->nodes = NULL;
    graph->edges = NULL;
    graph->node_count = 0;
    graph->edge_count = 0;
    
    return graph;
}

/**
 * Destroys a graph and frees all associated memory
 */
void graph_destroy(graph_t* graph) {
    if (graph == NULL) {
        return;
    }
    
    // Free all nodes
    graph_node_t* node = graph->nodes;
    while (node) {
        graph_node_t* next = node->next;
        free(node);
        node = next;
    }
    
    // Free all edges
    graph_edge_t* edge = graph->edges;
    while (edge) {
        graph_edge_t* next = edge->next;
        free(edge);
        edge = next;
    }
    
    free(graph);
}

/**
 * Finds a node in the graph with the given identifier, or NULL if not found
 */
static graph_node_t* graph_find_node(graph_t* graph, void* id) {
    if (graph == NULL) {
        return NULL;
    }
    
    graph_node_t* node = graph->nodes;
    while (node) {
        if (node->id == id) {
            return node;
        }
        node = node->next;
    }
    
    return NULL;
}

/**
 * Creates or retrieves a node in the graph with the given identifier
 */
graph_node_t* graph_find_or_create_node(graph_t* graph, void* id) {
    if (graph == NULL) {
        return NULL;
    }
    
    // Check if the node already exists
    graph_node_t* node = graph_find_node(graph, id);
    if (node != NULL) {
        return node;
    }
    
    // Create a new node
    node = malloc(sizeof(graph_node_t));
    if (node == NULL) {
        return NULL;
    }
    
    node->id = id;
    node->index = graph->node_count++;
    node->next = graph->nodes;
    graph->nodes = node;
    
    return node;
}

/**
 * Checks if an edge already exists between two nodes
 */
static bool graph_edge_exists(graph_t* graph, graph_node_t* from, graph_node_t* to) {
    if (graph == NULL || from == NULL || to == NULL) {
        return false;
    }
    
    graph_edge_t* edge = graph->edges;
    while (edge) {
        if (edge->from == from && edge->to == to) {
            return true;
        }
        edge = edge->next;
    }
    
    return false;
}

/**
 * Adds a directed edge between two nodes
 */
bool graph_add_edge(graph_t* graph, graph_node_t* from, graph_node_t* to) {
    if (graph == NULL || from == NULL || to == NULL) {
        return false;
    }
    
    // Check if the edge already exists
    if (graph_edge_exists(graph, from, to)) {
        return true;
    }
    
    // Create a new edge
    graph_edge_t* edge = malloc(sizeof(graph_edge_t));
    if (edge == NULL) {
        return false;
    }
    
    edge->from = from;
    edge->to = to;
    edge->next = graph->edges;
    graph->edges = edge;
    graph->edge_count++;
    
    return true;
}

/**
 * Depth-first search utility function for cycle detection
 */
static bool dfs_has_cycle(graph_t* graph, graph_node_t* node, graph_node_t* target, bool* visited) {
    // Mark current node as visited
    visited[node->index] = true;
    
    // Check all outgoing edges
    graph_edge_t* edge = graph->edges;
    while (edge) {
        if (edge->from == node) {
            // If we found the target, there's a cycle
            if (edge->to == target) {
                return true;
            }
            
            // If this node has not been visited, recurse
            if (!visited[edge->to->index]) {
                if (dfs_has_cycle(graph, edge->to, target, visited)) {
                    return true;
                }
            }
        }
        edge = edge->next;
    }
    
    return false;
}

/**
 * Checks if adding an edge would create a cycle in the graph
 */
bool graph_would_create_cycle(graph_t* graph, graph_node_t* from, graph_node_t* to) {
    if (graph == NULL || from == NULL || to == NULL) {
        return false;
    }
    
    // If adding an edge from 'to' back to 'from' would create a cycle,
    // then there's already a path from 'from' to 'to'
    
    // Allocate and initialize visited array
    bool* visited = calloc(graph->node_count, sizeof(bool));
    if (visited == NULL) {
        fprintf(stderr, "Failed to allocate memory for cycle detection\n");
        return false;  // Conservatively assume no cycle if we can't check
    }
    
    // Check if there's a path from 'to' to 'from'
    bool has_cycle = dfs_has_cycle(graph, to, from, visited);
    
    free(visited);
    return has_cycle;
}

/**
 * Performs cycle detection on the entire graph
 */
bool graph_has_cycle(graph_t* graph) {
    if (graph == NULL || graph->node_count == 0) {
        return false;
    }
    
    // For each node, check if there's a cycle starting from it
    graph_node_t* node = graph->nodes;
    while (node) {
        bool* visited = calloc(graph->node_count, sizeof(bool));
        if (visited == NULL) {
            fprintf(stderr, "Failed to allocate memory for cycle detection\n");
            return false;
        }
        
        if (dfs_has_cycle(graph, node, node, visited)) {
            free(visited);
            return true;
        }
        
        free(visited);
        node = node->next;
    }
    
    return false;
}

/**
 * Gets the number of nodes in the graph
 */
size_t graph_node_count(graph_t* graph) {
    return graph ? graph->node_count : 0;
}

/**
 * Gets the number of edges in the graph
 */
size_t graph_edge_count(graph_t* graph) {
    return graph ? graph->edge_count : 0;
}

/**
 * Gets the identifier associated with a node
 */
void* graph_node_get_id(graph_node_t* node) {
    return node ? node->id : NULL;
}

/**
 * Gets all nodes in the graph
 */
graph_node_t** graph_get_all_nodes(graph_t* graph, size_t* count) {
    if (graph == NULL || graph->node_count == 0 || count == NULL) {
        if (count) {
            *count = 0;
        }
        return NULL;
    }
    
    *count = graph->node_count;
    graph_node_t** nodes = malloc(graph->node_count * sizeof(graph_node_t*));
    if (nodes == NULL) {
        *count = 0;
        return NULL;
    }
    
    // Collect all nodes
    size_t i = 0;
    graph_node_t* node = graph->nodes;
    while (node && i < graph->node_count) {
        nodes[i++] = node;
        node = node->next;
    }
    
    return nodes;
}

/**
 * Gets all outgoing edges from a node
 */
graph_node_t** graph_get_outgoing_edges(graph_t* graph, graph_node_t* node, size_t* count) {
    if (graph == NULL || node == NULL || count == NULL) {
        if (count) {
            *count = 0;
        }
        return NULL;
    }
    
    // First pass: count outgoing edges
    size_t edge_count = 0;
    graph_edge_t* edge = graph->edges;
    while (edge) {
        if (edge->from == node) {
            edge_count++;
        }
        edge = edge->next;
    }
    
    if (edge_count == 0) {
        *count = 0;
        return NULL;
    }
    
    // Allocate array
    graph_node_t** destinations = malloc(edge_count * sizeof(graph_node_t*));
    if (destinations == NULL) {
        *count = 0;
        return NULL;
    }
    
    // Second pass: collect destinations
    size_t i = 0;
    edge = graph->edges;
    while (edge && i < edge_count) {
        if (edge->from == node) {
            destinations[i++] = edge->to;
        }
        edge = edge->next;
    }
    
    *count = edge_count;
    return destinations;
}

/**
 * Prints a representation of the graph for debugging
 */
void graph_print(graph_t* graph, const char* (*print_node_func)(void*)) {
    if (graph == NULL) {
        printf("Graph: NULL\n");
        return;
    }
    
    printf("Graph: %zu nodes, %zu edges\n", graph->node_count, graph->edge_count);
    
    // Print nodes
    printf("Nodes:\n");
    graph_node_t* node = graph->nodes;
    while (node) {
        const char* node_str = print_node_func ? print_node_func(node->id) : "?";
        printf("  Node %zu: %s\n", node->index, node_str ? node_str : "(null)");
        node = node->next;
    }
    
    // Print edges
    printf("Edges:\n");
    graph_edge_t* edge = graph->edges;
    while (edge) {
        const char* from_str = print_node_func ? print_node_func(edge->from->id) : "?";
        const char* to_str = print_node_func ? print_node_func(edge->to->id) : "?";
        printf("  %s -> %s\n", 
               from_str ? from_str : "(null)",
               to_str ? to_str : "(null)");
        edge = edge->next;
    }
}