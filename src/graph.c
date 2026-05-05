#include <stdio.h>
#include "graph.h"

int load_graph_from_file(const char *filename, Graph *graph, int *start, int *end) {
    FILE *file = fopen(filename, "r");

    if (!file) {
        perror("Error opening file");
        return 0;
    }

    if (fscanf(file, "%d %d", &graph->nodes, &graph->edges) != 2) {
        printf("Invalid input\n");
        fclose(file);
        return 0;
    }

    if (graph->nodes <= 0 || graph->nodes > MAX_NODES || graph->edges < 0) {
        printf("Invalid input\n");
        fclose(file);
        return 0;
    }

    for (int i = 0; i < graph->nodes; i++) {
        for (int j = 0; j < graph->nodes; j++) {
            graph->matrix[i][j] = INF;
        }
    }

    for (int i = 0; i < graph->nodes; i++) {
        graph->matrix[i][i] = 0;
    }

    for (int i = 0; i < graph->edges; i++) {
        int src, dst, weight;

        if (fscanf(file, "%d %d %d", &src, &dst, &weight) != 3) {
            printf("Invalid input\n");
            fclose(file);
            return 0;
        }

        if (src < 0 || dst < 0 || weight < 0 ||
            src >= graph->nodes || dst >= graph->nodes) {
            printf("Invalid input\n");
            fclose(file);
            return 0;
        }

        graph->matrix[src][dst] = weight;
    }

    if (fscanf(file, "%d %d", start, end) != 2) {
        printf("Invalid input\n");
        fclose(file);
        return 0;
    }

    if (*start < 0 || *end < 0 || *start >= graph->nodes || *end >= graph->nodes) {
        printf("Invalid input\n");
        fclose(file);
        return 0;
    }

    fclose(file);
    return 1;
}