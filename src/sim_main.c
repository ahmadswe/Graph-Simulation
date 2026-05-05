#include <stdio.h>
#include <math.h>
#include "raylib.h"
#include "graph.h"
#include "dijkstra.h"

#define WIDTH 900
#define HEIGHT 650
#define NODE_RADIUS 22
#define MOVE_STEP_TIME 0.3f
#define WAIT_TIME 1.0f

typedef enum {
    STATE_WAITING,
    STATE_MOVING,
    STATE_FINISHED
} AnimationState;

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

                char weight[16];
                sprintf(weight, "%d", graph->matrix[i][j]);
                DrawText(weight, mid.x, mid.y, 20, RED);
            }
        }
    }

    for (int i = 0; i < graph->nodes; i++) {
        DrawCircleV(positions[i], NODE_RADIUS, SKYBLUE);
        DrawCircleLines(positions[i].x, positions[i].y, NODE_RADIUS, BLUE);

        char text[16];
        sprintf(text, "%d", i);
        DrawText(text, positions[i].x - 6, positions[i].y - 10, 22, BLACK);
    }
}

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

    int path[MAX_NODES];
    int path_length = 0;
    int total_weight = 0;

    if (!dijkstra_path(&graph, start, end, path, &path_length, &total_weight)) {
        printf("No path found\n");
        return 1;
    }

    InitWindow(WIDTH, HEIGHT, "Graph Simulation - Milestone 3");
    SetTargetFPS(60);

    Vector2 positions[MAX_NODES];
    float radius = 220;
    Vector2 center = {WIDTH / 2, HEIGHT / 2};

    for (int i = 0; i < graph.nodes; i++) {
        float angle = (2 * PI * i) / graph.nodes;
        positions[i] = (Vector2){
            center.x + radius * cosf(angle),
            center.y + radius * sinf(angle)
        };
    }

    Rectangle button = {20, 20, 120, 40};

    int isPlaying = 0;
    AnimationState state = STATE_WAITING;

    int path_index = 0;
    int edge_step = 0;
    float timer = 0.0f;

    Vector2 traveler = positions[path[0]];

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            Vector2 mouse = GetMousePosition();

            if (CheckCollisionPointRec(mouse, button)) {
                isPlaying = !isPlaying;
            }
        }

        if (isPlaying && state != STATE_FINISHED) {
            timer += dt;

            if (state == STATE_WAITING) {
                int at_source = (path_index == 0);
                int at_destination = (path_index == path_length - 1);

                if (at_source || at_destination) {
                    state = STATE_MOVING;
                    timer = 0.0f;
                } else if (timer >= WAIT_TIME) {
                    state = STATE_MOVING;
                    timer = 0.0f;
                }
            }

            if (state == STATE_MOVING && path_index < path_length - 1) {
                int from = path[path_index];
                int to = path[path_index + 1];
                int weight = graph.matrix[from][to];

                if (timer >= MOVE_STEP_TIME) {
                    timer = 0.0f;
                    edge_step++;

                    float t = (float)edge_step / weight;

                    traveler.x = positions[from].x + (positions[to].x - positions[from].x) * t;
                    traveler.y = positions[from].y + (positions[to].y - positions[from].y) * t;

                    if (edge_step >= weight) {
                        traveler = positions[to];
                        path_index++;
                        edge_step = 0;

                        if (path_index == path_length - 1) {
                            state = STATE_FINISHED;
                            isPlaying = 0;
                        } else {
                            state = STATE_WAITING;
                            timer = 0.0f;
                        }
                    }
                }
            }
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);

        draw_graph(&graph, positions);

        DrawRectangleRec(button, isPlaying ? ORANGE : GREEN);
        DrawText(isPlaying ? "STOP" : "PLAY", button.x + 32, button.y + 10, 22, BLACK);

        DrawText("Dijkstra path:", 20, 80, 20, BLACK);

        int x = 20;
        for (int i = 0; i < path_length; i++) {
            char nodeText[16];
            sprintf(nodeText, "%d", path[i]);
            DrawText(nodeText, x, 110, 20, DARKBLUE);
            x += 20;

            if (i < path_length - 1) {
                DrawText("->", x, 110, 20, DARKBLUE);
                x += 30;
            }
        }

        char weightText[64];
        sprintf(weightText, "Total weight: %d", total_weight);
        DrawText(weightText, 20, 140, 20, BLACK);

        DrawCircleV(traveler, 12, GREEN);
        DrawCircleLines(traveler.x, traveler.y, 12, DARKGREEN);

        if (state == STATE_FINISHED) {
            DrawText("Arrived at destination!", 20, 180, 24, MAROON);
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}