#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include "raylib.h"
#include "graph.h"
#include "dijkstra.h"
#include "draw_utils.h"

#define MOVE_STEP_TIME 0.3f
#define NODE_STAY_TIME 1

typedef enum { SCHED_FCFS, SCHED_SJF } SchedAlgo;

/* Message types child -> parent */
#define MSG_WAITING  0   /* want to enter node (blocked until parent signals)  */
#define MSG_ENTERED  1   /* parent granted entry, now inside node               */
#define MSG_MOVING   2   /* released node, traversing edge                      */
#define MSG_FINISHED 3   /* journey complete                                    */
#define MSG_LEAVING  4   /* about to leave node (parent may schedule next)      */

typedef struct {
    int type;
    int node;
    int next_node;
    int priority;   /* remaining path weight for SJF; unused for FCFS */
} TravelerMsg;

typedef enum { T_IDLE, T_WAITING, T_IN_NODE, T_MOVING, T_AT_DEST, T_FINISHED } T7State;

#define QSIZE 64
typedef struct { TravelerMsg buf[QSIZE]; int head, tail; } MsgQueue;

static void q_push(MsgQueue *q, TravelerMsg m) { q->buf[q->tail % QSIZE] = m; q->tail++; }
static int  q_pop(MsgQueue *q, TravelerMsg *m) {
    if (q->head == q->tail) return 0;
    *m = q->buf[q->head++ % QSIZE]; return 1;
}

typedef struct {
    int traveler_idx;
    int priority;   /* lower value = served first */
} WaitEntry;

typedef struct {
    WaitEntry entries[MAX_TRAVELERS];
    int count;
    int busy;   /* 1 if a traveler is currently inside this node */
} NodeQueue;

typedef struct {
    int      src, dst;
    pid_t    pid;
    Color    color;
    int      read_fd;   /* child->parent data pipe  (read end in parent)  */
    int      signal_fd; /* parent->child signal pipe (write end in parent) */
    T7State  state;
    int      at_node;
    int      target_node;
    int      edge_weight;
    int      edge_step;
    float    timer;
    Vector2  position;
    MsgQueue queue;
    long long wait_start_ns;
    long long total_wait_ns;
    int       wait_count;
} Traveler7;

static Color palette[]    = {GREEN, ORANGE, PURPLE, YELLOW, PINK, RED, BLUE, BROWN};
static int   palette_size = 8;

static SchedAlgo g_sched;
static NodeQueue g_node_queues[MAX_NODES];
static int       g_signal_wfds[MAX_TRAVELERS]; /* parent writes here to wake child i */
static int       g_arrival_seq = 0;

static long long now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

/* Wake the best-priority waiting traveler for this node */
static void schedule_next(int node) {
    NodeQueue *nq = &g_node_queues[node];
    if (nq->count == 0 || nq->busy) return;

    int best = 0;
    for (int i = 1; i < nq->count; i++)
        if (nq->entries[i].priority < nq->entries[best].priority)
            best = i;

    int tidx = nq->entries[best].traveler_idx;
    for (int i = best; i < nq->count - 1; i++)
        nq->entries[i] = nq->entries[i + 1];
    nq->count--;
    nq->busy = 1;

    char go = 1;
    write(g_signal_wfds[tidx], &go, 1);
}

/* ------------------------------------------------------------------ */
/* Child process: compute path, traverse with parent-controlled entry  */
/* ------------------------------------------------------------------ */
static void child_run(int write_fd, int read_fd, const Graph *g, int src, int dst) {
    int path[MAX_NODES], len, total_w;
    if (!dijkstra_path(g, src, dst, path, &len, &total_w)) {
        TravelerMsg m = {MSG_FINISHED, -1, -1, 0};
        write(write_fd, &m, sizeof(m));
        close(write_fd);
        close(read_fd);
        _exit(0);
    }

    /* Precompute remaining path weight from each position (used as SJF priority) */
    int remaining[MAX_NODES];
    remaining[len - 1] = 0;
    for (int i = len - 2; i >= 0; i--)
        remaining[i] = g->matrix[path[i]][path[i + 1]] + remaining[i + 1];

    for (int i = 0; i < len; i++) {
        int node = path[i];
        int next = (i < len - 1) ? path[i + 1] : -1;

        /* Notify parent: waiting to enter node */
        TravelerMsg m = {MSG_WAITING, node, next, remaining[i]};
        write(write_fd, &m, sizeof(m));

        /* Block until parent grants entry (scheduler decides order) */
        char go;
        read(read_fd, &go, 1);

        /* Notify parent: now inside node */
        m.type = MSG_ENTERED;
        write(write_fd, &m, sizeof(m));

        sleep(NODE_STAY_TIME);

        /* Notify parent: leaving node so next waiter can be scheduled */
        m.type = MSG_LEAVING;
        write(write_fd, &m, sizeof(m));

        if (next != -1) {
            int ew = g->matrix[node][next];
            m = (TravelerMsg){MSG_MOVING, node, next, 0};
            write(write_fd, &m, sizeof(m));
            usleep((long)ew * (long)(MOVE_STEP_TIME * 1000000));
        }
    }

    TravelerMsg done = {MSG_FINISHED, path[len - 1], -1, 0};
    write(write_fd, &done, sizeof(done));
    close(write_fd);
    close(read_fd);
    _exit(0);
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */
int main(int argc, char *argv[]) {
    if (argc < 4 || strcmp(argv[1], "-schd") != 0) {
        printf("Usage: ./sim -schd fcfs <file>\n");
        printf("       ./sim -schd sjf  <file>\n");
        return 1;
    }
    if      (strcmp(argv[2], "fcfs") == 0) g_sched = SCHED_FCFS;
    else if (strcmp(argv[2], "sjf")  == 0) g_sched = SCHED_SJF;
    else {
        printf("Unknown scheduler '%s'. Use 'fcfs' or 'sjf'.\n", argv[2]);
        return 1;
    }

    Graph graph;
    TravelerSpec specs[MAX_TRAVELERS];
    int nt = 0;
    if (!load_graph_with_travelers(argv[3], &graph, specs, &nt)) return 1;

    memset(g_node_queues, 0, sizeof(g_node_queues));

    /* Create all pipes before forking */
    int data_pfds[MAX_TRAVELERS][2];
    int  sig_pfds[MAX_TRAVELERS][2];
    for (int i = 0; i < nt; i++) {
        if (pipe(data_pfds[i]) < 0 || pipe(sig_pfds[i]) < 0) { perror("pipe"); return 1; }
    }

    Traveler7 travelers[MAX_TRAVELERS];

    for (int i = 0; i < nt; i++) {
        Traveler7 *t     = &travelers[i];
        t->src           = specs[i].src;
        t->dst           = specs[i].dst;
        t->color         = palette[i % palette_size];
        t->state         = T_IDLE;
        t->at_node       = specs[i].src;
        t->edge_step     = 0;
        t->timer         = 0.0f;
        t->queue.head    = t->queue.tail = 0;
        t->wait_start_ns = 0;
        t->total_wait_ns = 0;
        t->wait_count    = 0;
        g_signal_wfds[i] = sig_pfds[i][1];

        pid_t pid = fork();
        if (pid == 0) {
            /* Close all pipe ends the child doesn't own */
            for (int j = 0; j < nt; j++) {
                close(data_pfds[j][0]); /* parent read */
                close(sig_pfds[j][1]);  /* parent write */
                if (j != i) {
                    close(data_pfds[j][1]); /* other child write */
                    close(sig_pfds[j][0]);  /* other child read  */
                }
            }
            child_run(data_pfds[i][1], sig_pfds[i][0], &graph, specs[i].src, specs[i].dst);
        }
        if (pid < 0) { perror("fork"); return 1; }
        t->pid       = pid;
        t->read_fd   = data_pfds[i][0];
        t->signal_fd = sig_pfds[i][1];
        close(data_pfds[i][1]); /* child write end not needed in parent */
        close(sig_pfds[i][0]);  /* child read  end not needed in parent */
        fcntl(t->read_fd, F_SETFL, O_NONBLOCK);
    }

    const char *algo_name = (g_sched == SCHED_FCFS) ? "FCFS" : "SJF";
    char title[80];
    snprintf(title, sizeof(title),
             "Graph Simulation – Milestone 7  |  Scheduler: %s", algo_name);
    InitWindow(WIDTH, HEIGHT, title);
    SetTargetFPS(60);

    Vector2 positions[MAX_NODES];
    init_node_positions(&graph, positions);
    for (int i = 0; i < nt; i++)
        travelers[i].position = positions[travelers[i].src];

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        /* 1. Drain pipes – scheduling messages are handled immediately */
        for (int i = 0; i < nt; i++) {
            Traveler7 *t = &travelers[i];
            if (t->state == T_FINISHED) continue;
            TravelerMsg msg;
            while (read(t->read_fd, &msg, sizeof(msg)) == (ssize_t)sizeof(msg)) {
                if (msg.type == MSG_WAITING) {
                    t->state         = T_WAITING;
                    t->wait_start_ns = now_ns();
                    int prio = (g_sched == SCHED_SJF) ? msg.priority : g_arrival_seq++;
                    NodeQueue *nq    = &g_node_queues[msg.node];
                    nq->entries[nq->count++] = (WaitEntry){i, prio};
                    schedule_next(msg.node);
                } else if (msg.type == MSG_LEAVING) {
                    g_node_queues[msg.node].busy = 0;
                    schedule_next(msg.node);
                } else {
                    q_push(&t->queue, msg);
                }
            }
        }

        /* 2. Advance edge animations */
        for (int i = 0; i < nt; i++) {
            Traveler7 *t = &travelers[i];
            if (t->state != T_MOVING) continue;
            t->timer += dt;
            if (t->timer >= MOVE_STEP_TIME) {
                t->timer = 0.0f;
                t->edge_step++;
                float p = (float)t->edge_step / t->edge_weight;
                t->position.x = positions[t->at_node].x +
                    (positions[t->target_node].x - positions[t->at_node].x) * p;
                t->position.y = positions[t->at_node].y +
                    (positions[t->target_node].y - positions[t->at_node].y) * p;
                if (t->edge_step >= t->edge_weight) {
                    t->position = positions[t->target_node];
                    t->state    = T_IDLE;
                }
            }
        }

        /* 3. Process queued GUI messages */
        for (int i = 0; i < nt; i++) {
            Traveler7 *t = &travelers[i];
            if (t->state == T_MOVING || t->state == T_FINISHED) continue;
            TravelerMsg msg;
            while (q_pop(&t->queue, &msg)) {
                switch (msg.type) {
                case MSG_ENTERED:
                    if (t->wait_start_ns > 0) {
                        t->total_wait_ns += now_ns() - t->wait_start_ns;
                        t->wait_count++;
                        t->wait_start_ns = 0;
                    }
                    printf("[PID=%d] entered node %d", (int)t->pid, msg.node);
                    if (msg.next_node == -1) printf(" | DESTINATION");
                    else                     printf(" | next: %d", msg.next_node);
                    printf("\n"); fflush(stdout);
                    t->at_node  = msg.node;
                    t->position = positions[msg.node];
                    t->state    = (msg.next_node == -1) ? T_AT_DEST : T_IN_NODE;
                    break;
                case MSG_MOVING:
                    t->at_node     = msg.node;
                    t->target_node = msg.next_node;
                    t->edge_weight = graph.matrix[msg.node][msg.next_node];
                    t->edge_step   = 0;
                    t->timer       = 0.0f;
                    t->state       = T_MOVING;
                    break;
                case MSG_FINISHED:
                    printf("[PID=%d] finished  (total wait: %.0f ms across %d nodes)\n",
                           (int)t->pid,
                           (double)t->total_wait_ns / 1e6,
                           t->wait_count);
                    fflush(stdout);
                    t->state = T_FINISHED;
                    break;
                }
                if (t->state == T_MOVING) break;
            }
        }

        /* 4. Draw */
        BeginDrawing();
        ClearBackground(RAYWHITE);
        draw_graph(&graph, positions);

        /* Scheduler banner */
        Color banner_col = (g_sched == SCHED_FCFS) ? DARKBLUE : DARKGREEN;
        DrawRectangle(0, 0, 220, 32, banner_col);
        char banner[48];
        snprintf(banner, sizeof(banner), "Scheduler: %s", algo_name);
        DrawText(banner, 8, 7, 20, WHITE);

        /* Waiting queue count per node */
        for (int n = 0; n < graph.nodes; n++) {
            if (g_node_queues[n].count > 0) {
                char ql[12];
                snprintf(ql, sizeof(ql), "Q:%d", g_node_queues[n].count);
                DrawText(ql, (int)positions[n].x + 26,
                         (int)positions[n].y - 22, 16, MAROON);
            }
        }

        /* Travelers */
        for (int i = 0; i < nt; i++) {
            Traveler7 *t = &travelers[i];
            Color c = (t->state == T_WAITING) ? GRAY : t->color;
            DrawCircleV(t->position, 12, c);
            DrawCircleLines((int)t->position.x, (int)t->position.y, 12, BLACK);
            char lbl[16];
            snprintf(lbl, sizeof(lbl), "%d", i);
            DrawText(lbl, (int)t->position.x - 5, (int)t->position.y - 8, 18, BLACK);
            if (t->state == T_WAITING)
                DrawText("W", (int)t->position.x - 4,
                         (int)t->position.y - 30, 16, RED);
        }

        EndDrawing();
    }

    CloseWindow();

    for (int i = 0; i < nt; i++) {
        close(travelers[i].read_fd);
        close(travelers[i].signal_fd);
        waitpid(travelers[i].pid, NULL, 0);
    }
    return 0;
}
