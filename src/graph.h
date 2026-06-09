#ifndef GRAPH_H
#define GRAPH_H

#define INF 1000000000
#define MAX_NODES 15

typedef struct {
    int nodes;
    int edges;
    int matrix[MAX_NODES][MAX_NODES];
} Graph;

int load_graph_from_file(const char *filename, Graph *graph, int *start, int *end);

#define MAX_TRAVELERS 10

typedef struct {
    int src;
    int dst;
} TravelerSpec;

int load_graph_with_travelers(const char *filename, Graph *graph, TravelerSpec travelers[], int *num_travelers);

#endif