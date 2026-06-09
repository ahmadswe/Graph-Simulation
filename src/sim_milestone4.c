#include <stdio.h>
#include <math.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include "raylib.h"
#include "graph.h"
#include "dijkstra.h"
#include "draw_utils.h"

#define MOVE_STEP_TIME 0.3f
#define WAIT_TIME      1.0f

typedef enum {
    STATE_WAITING,
    STATE_MOVING,
    STATE_FINISHED
} AnimState;

typedef struct {
    int      src, dst;
    int      path[MAX_NODES];
    int      path_length;
    int      total_weight;
    pid_t    pid;
    int      child_active;
    Color    color;
    int      path_index;
    int      edge_step;
    float    timer;
    AnimState state;
    Vector2  position;
} Traveler;

static Color palette[] = {GREEN, ORANGE, PURPLE, YELLOW, PINK, RED, BLUE, BROWN};
static int palette_size = 8;

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: ./sim <file>\n");
        return 1;
    }

    Graph graph;
    TravelerSpec specs[MAX_TRAVELERS];
    int num_travelers = 0;

    if (!load_graph_with_travelers(argv[1], &graph, specs, &num_travelers))
        return 1;

    Traveler travelers[MAX_TRAVELERS];

    for (int i = 0; i < num_travelers; i++) {
        Traveler *t = &travelers[i];
        t->src          = specs[i].src;
        t->dst          = specs[i].dst;
        t->color        = palette[i % palette_size];
        t->path_index   = 0;
        t->edge_step    = 0;
        t->timer        = 0.0f;
        t->child_active = 0;

        if (!dijkstra_path(&graph, t->src, t->dst, t->path, &t->path_length, &t->total_weight)) {
            printf("No path found for traveler %d (%d -> %d)\n", i, t->src, t->dst);
            t->state = STATE_FINISHED;
        } else {
            t->state = STATE_WAITING;
        }
    }

    /* Fork children BEFORE opening the window */
    for (int i = 0; i < num_travelers; i++) {
        if (travelers[i].state == STATE_FINISHED)
            continue;

        pid_t pid = fork();
        if (pid == 0) {
            printf("[%d] started\n", getpid());
            fflush(stdout);
            pause();
            _exit(0);
        }
        if (pid < 0) {
            perror("fork");
            return 1;
        }
        travelers[i].pid          = pid;
        travelers[i].child_active = 1;
    }

    InitWindow(WIDTH, HEIGHT, "Graph Simulation - Milestone 4");
    SetTargetFPS(60);

    Vector2 positions[MAX_NODES];
    init_node_positions(&graph, positions);

    for (int i = 0; i < num_travelers; i++)
        if (travelers[i].path_length > 0)
            travelers[i].position = positions[travelers[i].path[0]];

    Rectangle btn = {20, 20, 120, 40};
    int playing = 0;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            Vector2 mouse = GetMousePosition();
            if (CheckCollisionPointRec(mouse, btn))
                playing = !playing;
        }

        if (playing) {
            for (int i = 0; i < num_travelers; i++) {
                Traveler *t = &travelers[i];
                if (t->state == STATE_FINISHED) continue;

                t->timer += dt;

                if (t->state == STATE_WAITING) {
                    int at_end = (t->path_index == 0 || t->path_index == t->path_length - 1);
                    if (at_end || t->timer >= WAIT_TIME) {
                        t->state = STATE_MOVING;
                        t->timer = 0.0f;
                    }
                }

                if (t->state == STATE_MOVING && t->path_index < t->path_length - 1) {
                    int from   = t->path[t->path_index];
                    int to     = t->path[t->path_index + 1];
                    int weight = graph.matrix[from][to];

                    if (t->timer >= MOVE_STEP_TIME) {
                        t->timer = 0.0f;
                        t->edge_step++;

                        float progress = (float)t->edge_step / weight;
                        t->position.x  = positions[from].x + (positions[to].x - positions[from].x) * progress;
                        t->position.y  = positions[from].y + (positions[to].y - positions[from].y) * progress;

                        if (t->edge_step >= weight) {
                            t->position   = positions[to];
                            t->path_index++;
                            t->edge_step  = 0;

                            if (t->path_index == t->path_length - 1) {
                                t->state = STATE_FINISHED;
                                if (t->child_active) {
                                    kill(t->pid, SIGTERM);
                                    t->child_active = 0;
                                }
                            } else {
                                t->state = STATE_WAITING;
                                t->timer = 0.0f;
                            }
                        }
                    }
                }
            }

            /* Stop playing when all are done */
            int all_done = 1;
            for (int i = 0; i < num_travelers; i++)
                if (travelers[i].state != STATE_FINISHED) { all_done = 0; break; }
            if (all_done) playing = 0;
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);

        draw_graph(&graph, positions);

        DrawRectangleRec(btn, playing ? ORANGE : GREEN);
        DrawText(playing ? "STOP" : "PLAY", (int)btn.x + 32, (int)btn.y + 10, 22, BLACK);

        for (int i = 0; i < num_travelers; i++) {
            Traveler *t = &travelers[i];
            DrawCircleV(t->position, 12, t->color);
            DrawCircleLines((int)t->position.x, (int)t->position.y, 12, BLACK);
            char lbl[16];
            sprintf(lbl, "%d", i);
            DrawText(lbl, (int)t->position.x - 5, (int)t->position.y - 8, 18, BLACK);
        }

        EndDrawing();
    }

    CloseWindow();

    /* Kill any still-running children and reap all */
    for (int i = 0; i < num_travelers; i++) {
        if (travelers[i].child_active)
            kill(travelers[i].pid, SIGTERM);
        if (travelers[i].pid > 0)
            waitpid(travelers[i].pid, NULL, 0);
    }

    return 0;
}
