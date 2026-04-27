#include <stdio.h>
#include <stdlib.h>

#define INF 1000000000

void dijkstra(int N, int graph[N][N], int start, int end) {
    int dist[N];
    int visited[N];
    int parent[N];

    for (int i = 0; i < N; i++) {
        dist[i] = INF;
        visited[i] = 0;
        parent[i] = -1;
    }

    dist[start] = 0;

    for (int i = 0; i < N; i++) {
        int u = -1;

        for (int j = 0; j < N; j++) {
            if (!visited[j] && (u == -1 || dist[j] < dist[u])) {
                u = j;
            }
        }

        if (dist[u] == INF) break;

        visited[u] = 1;

        for (int v = 0; v < N; v++) {
            if (graph[u][v] != INF && !visited[v]) {
                if (dist[u] + graph[u][v] < dist[v]) {
                    dist[v] = dist[u] + graph[u][v];
                    parent[v] = u;
                }
            }
        }
    }

    if (dist[end] == INF) {
        printf("No path found\n");
        return;
    }

    // הדפסת מסלול
    int path[N];
    int count = 0;

    for (int v = end; v != -1; v = parent[v]) {
        path[count++] = v;
    }

    for (int i = count - 1; i >= 0; i--) {
        printf("%d", path[i]);
        if (i > 0) printf(" -> ");
    }

    printf("\n%d\n", dist[end]);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: ./dijkstra <file>\n");
        return 1;
    }

    FILE *file = fopen(argv[1], "r");
    if (!file) {
        perror("Error opening file");
        return 1;
    }

    int N, M;
    fscanf(file, "%d %d", &N, &M);

    int graph[N][N];

    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            graph[i][j] = INF;
        }
    }

    for (int i = 0; i < N; i++) {
        graph[i][i] = 0;
    }

    int src, dst, weight;

    for (int i = 0; i < M; i++) {
        fscanf(file, "%d %d %d", &src, &dst, &weight);

        if (src < 0 || dst < 0 || weight < 0 || src >= N || dst >= N) {
            printf("Invalid input\n");
            fclose(file);
            return 1;
        }

        graph[src][dst] = weight;
    }

    int start, end;
    fscanf(file, "%d %d", &start, &end);

    fclose(file);


    dijkstra(N, graph, start, end);

    return 0;
}