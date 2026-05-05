#include <stdio.h>
#include "graph.h"
#include "dijkstra.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: ./dijkstra <file>\n");
        return 1;
    }

    Graph graph;
    int start, end;

    if (!load_graph_from_file(argv[1], &graph, &start, &end)) {
        return 1;
    }

    dijkstra(&graph, start, end);

    return 0;
}