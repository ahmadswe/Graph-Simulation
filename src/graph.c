/*
 * ===================================================================
 * graph.c - טעינת גרף מקובץ
 * ===================================================================
 * load_graph_from_file: Milestone 3 - קורא גרף עם start/end בודד
 *   פורמט קובץ:
 *     שורה 1: <nodes> <edges>
 *     שורות 2..edges+1: <src> <dst> <weight>
 *     שורה אחרונה: <start> <end>
 *
 * load_graph_with_travelers: Milestones 4-7 - קורא גרף עם מספר נוסעים
 *   פורמט קובץ:
 *     שורה 1: <nodes> <edges>
 *     שורות 2..edges+1: <src> <dst> <weight>
 *     שורה N: <num_travelers>
 *     שורות N+1..: <src> <dst> לכל נוסע
 *     (# = שורת הערה, מדולגת)
 * ===================================================================
 */

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

int load_graph_with_travelers(const char *filename, Graph *graph, TravelerSpec travelers[], int *num_travelers) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening file");
        return 0;
    }

    for (int i = 0; i < MAX_NODES; i++)
        for (int j = 0; j < MAX_NODES; j++)
            graph->matrix[i][j] = (i == j) ? 0 : INF;

    /* state: 0=header 1=edges 2=num_travelers 3=traveler_pairs */
    int state = 0;
    int edge_count = 0;
    int tcount = 0;
    char line[256];

    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r')
            continue;

        if (state == 0) {
            if (sscanf(line, "%d %d", &graph->nodes, &graph->edges) != 2 ||
                graph->nodes <= 0 || graph->nodes > MAX_NODES || graph->edges < 0) {
                printf("Invalid input\n");
                fclose(file);
                return 0;
            }
            state = 1;
        } else if (state == 1) {
            int src, dst, weight;
            if (sscanf(line, "%d %d %d", &src, &dst, &weight) != 3 ||
                src < 0 || dst < 0 || weight < 0 ||
                src >= graph->nodes || dst >= graph->nodes) {
                printf("Invalid input\n");
                fclose(file);
                return 0;
            }
            graph->matrix[src][dst] = weight;
            if (++edge_count >= graph->edges)
                state = 2;
        } else if (state == 2) {
            if (sscanf(line, "%d", num_travelers) != 1 || *num_travelers <= 0) {
                printf("Invalid input\n");
                fclose(file);
                return 0;
            }
            state = 3;
        } else {
            if (sscanf(line, "%d %d", &travelers[tcount].src, &travelers[tcount].dst) != 2 ||
                travelers[tcount].src < 0 || travelers[tcount].dst < 0 ||
                travelers[tcount].src >= graph->nodes || travelers[tcount].dst >= graph->nodes) {
                printf("Invalid input\n");
                fclose(file);
                return 0;
            }
            if (++tcount >= *num_travelers)
                break;
        }
    }

    fclose(file);
    return 1;
}