#ifndef DRAW_UTILS_H
#define DRAW_UTILS_H

#include <math.h>
#include <stdio.h>
#include "raylib.h"
#include "graph.h"

#define WIDTH 900
#define HEIGHT 650
#define NODE_RADIUS 22

static void draw_arrow(Vector2 from, Vector2 to) {
    Vector2 dir = {to.x - from.x, to.y - from.y};
    float length = sqrtf(dir.x * dir.x + dir.y * dir.y);
    if (length == 0) return;
    dir.x /= length;
    dir.y /= length;

    Vector2 end = {
        to.x - dir.x * (NODE_RADIUS + 3),
        to.y - dir.y * (NODE_RADIUS + 3)
    };
    DrawLineV(from, end, BLACK);

    float as = 10;
    Vector2 left  = {end.x - dir.x * as + dir.y * as, end.y - dir.y * as - dir.x * as};
    Vector2 right = {end.x - dir.x * as - dir.y * as, end.y - dir.y * as + dir.x * as};
    DrawTriangle(end, left, right, BLACK);
}

static void draw_graph(const Graph *graph, Vector2 positions[]) {
    for (int i = 0; i < graph->nodes; i++) {
        for (int j = 0; j < graph->nodes; j++) {
            if (graph->matrix[i][j] != INF && i != j) {
                draw_arrow(positions[i], positions[j]);
                Vector2 mid = {
                    (positions[i].x + positions[j].x) / 2 + 6,
                    (positions[i].y + positions[j].y) / 2 - 6
                };
                char w[16];
                sprintf(w, "%d", graph->matrix[i][j]);
                DrawText(w, (int)mid.x, (int)mid.y, 20, RED);
            }
        }
    }
    for (int i = 0; i < graph->nodes; i++) {
        DrawCircleV(positions[i], NODE_RADIUS, SKYBLUE);
        DrawCircleLines((int)positions[i].x, (int)positions[i].y, NODE_RADIUS, BLUE);
        char t[16];
        sprintf(t, "%d", i);
        DrawText(t, (int)positions[i].x - 6, (int)positions[i].y - 10, 22, BLACK);
    }
}

static void init_node_positions(const Graph *graph, Vector2 positions[]) {
    Vector2 center = {WIDTH / 2.0f, HEIGHT / 2.0f};
    for (int i = 0; i < graph->nodes; i++) {
        float angle = (2.0f * PI * i) / graph->nodes;
        positions[i] = (Vector2){
            center.x + 220.0f * cosf(angle),
            center.y + 220.0f * sinf(angle)
        };
    }
}

#endif
