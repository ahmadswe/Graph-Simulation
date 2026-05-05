#include <stdio.h>
#include "raylib.h"
#include "graph.h"

#define WIDTH 800
#define HEIGHT 600

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: ./sim <file>\n");
        return 1;
    }

    Graph graph;
    int start, end;

    if (!load_graph_from_file(argv[1], &graph, &start, &end)) {
        return 1;
    }

    InitWindow(WIDTH, HEIGHT, "Graph Simulation");
    SetTargetFPS(60);

    Vector2 positions[MAX_NODES];

    for (int i = 0; i < graph.nodes; i++) {
        float radius = 200;
Vector2 center = {WIDTH / 2, HEIGHT / 2};

for (int i = 0; i < graph.nodes; i++) {
    float angle = (2 * PI * i) / graph.nodes;

    positions[i] = (Vector2){
        center.x + radius * cosf(angle),
        center.y + radius * sinf(angle)
    };
}
    }

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        // Draw edges
        for (int i = 0; i < graph.nodes; i++) {
            for (int j = 0; j < graph.nodes; j++) {
                if (graph.matrix[i][j] != INF && i != j) {
                    DrawLineV(positions[i], positions[j], BLACK);
                    Vector2 dir = {
                        positions[j].x - positions[i].x,
                        positions[j].y - positions[i].y
                    };
                    
                    // נורמליזציה
                    float length = sqrtf(dir.x * dir.x + dir.y * dir.y);
                    dir.x /= length;
                    dir.y /= length;
                    
                    // נקודת סוף (לפני הצומת)
                    float nodeRadius = 22;

                    Vector2 end = {
                        positions[j].x - dir.x * (nodeRadius + 2),
                        positions[j].y - dir.y * (nodeRadius + 2)
                    };
                    
                    // ציור ראש חץ
                    float arrowSize = 10;
                    
                    Vector2 left = {
                        end.x - dir.x * arrowSize + dir.y * arrowSize,
                        end.y - dir.y * arrowSize - dir.x * arrowSize
                    };
                    
                    Vector2 right = {
                        end.x - dir.x * arrowSize - dir.y * arrowSize,
                        end.y - dir.y * arrowSize + dir.x * arrowSize
                    };
                    
                    DrawTriangle(end, left, right, BLACK);
                    Vector2 mid = {
                        (positions[i].x + positions[j].x) / 2,
                        (positions[i].y + positions[j].y) / 2
                    };

                    char weight[10];
                    sprintf(weight, "%d", graph.matrix[i][j]);

                    DrawText(weight, mid.x, mid.y - 20, 20, RED);
                }
            }
        }

        // Draw nodes
        for (int i = 0; i < graph.nodes; i++) {
            DrawCircleV(positions[i], 22, SKYBLUE);
            DrawCircleLines(positions[i].x, positions[i].y, 22, BLUE);

            char text[10];
            sprintf(text, "%d", i);

            DrawText(text, positions[i].x - 6, positions[i].y - 10, 22, BLACK);
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}