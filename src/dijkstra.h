#ifndef DIJKSTRA_H
#define DIJKSTRA_H

#include "graph.h"

/*
 * Dijkstra shortest-path API declarations.
 * Provides a console-printing function and a reusable path extraction helper.
 */

void dijkstra(const Graph *graph, int start, int end);

int dijkstra_path(const Graph *graph, int start, int end, int path[], int *path_length, int *total_weight);

#endif