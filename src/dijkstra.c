#include <stdio.h>
#include "dijkstra.h"

void dijkstra(const Graph *graph, int start, int end) {
    int dist[MAX_NODES];
    int visited[MAX_NODES];
    int parent[MAX_NODES];

    for (int i = 0; i < graph->nodes; i++) {
        dist[i] = INF;
        visited[i] = 0;
        parent[i] = -1;
    }

    dist[start] = 0;

    for (int i = 0; i < graph->nodes; i++) {
        int u = -1;

        for (int j = 0; j < graph->nodes; j++) {
            if (!visited[j] && (u == -1 || dist[j] < dist[u])) {
                u = j;
            }
        }

        if (u == -1 || dist[u] == INF) {
            break;
        }

        visited[u] = 1;

        for (int v = 0; v < graph->nodes; v++) {
            if (graph->matrix[u][v] != INF && !visited[v]) {
                if (dist[u] + graph->matrix[u][v] < dist[v]) {
                    dist[v] = dist[u] + graph->matrix[u][v];
                    parent[v] = u;
                }
            }
        }
    }

    if (dist[end] == INF) {
        printf("No path found\n");
        return;
    }

    int path[MAX_NODES];
    int count = 0;

    for (int v = end; v != -1; v = parent[v]) {
        path[count++] = v;
    }

    for (int i = count - 1; i >= 0; i--) {
        printf("%d", path[i]);
        if (i > 0) {
            printf(" -> ");
        }
    }

    printf("\n%d\n", dist[end]);
}
int dijkstra_path(const Graph *graph, int start, int end, int path[], int *path_length, int *total_weight) {
    int dist[MAX_NODES];
    int visited[MAX_NODES];
    int parent[MAX_NODES];

    for (int i = 0; i < graph->nodes; i++) {
        dist[i] = INF;
        visited[i] = 0;
        parent[i] = -1;
    }

    dist[start] = 0;

    for (int i = 0; i < graph->nodes; i++) {
        int u = -1;

        for (int j = 0; j < graph->nodes; j++) {
            if (!visited[j] && (u == -1 || dist[j] < dist[u])) {
                u = j;
            }
        }

        if (u == -1 || dist[u] == INF) {
            break;
        }

        visited[u] = 1;

        for (int v = 0; v < graph->nodes; v++) {
            if (graph->matrix[u][v] != INF && !visited[v]) {
                if (dist[u] + graph->matrix[u][v] < dist[v]) {
                    dist[v] = dist[u] + graph->matrix[u][v];
                    parent[v] = u;
                }
            }
        }
    }

    if (dist[end] == INF) {
        *path_length = 0;
        *total_weight = INF;
        return 0;
    }

    int temp[MAX_NODES];
    int count = 0;

    for (int v = end; v != -1; v = parent[v]) {
        temp[count++] = v;
    }

    *path_length = count;
    *total_weight = dist[end];

    for (int i = 0; i < count; i++) {
        path[i] = temp[count - 1 - i];
    }

    return 1;
}