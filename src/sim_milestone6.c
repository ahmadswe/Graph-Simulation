#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <semaphore.h>
#include "raylib.h"
#include "graph.h"
#include "dijkstra.h"
#include "draw_utils.h"

#define MOVE_STEP_TIME 0.3f
#define NODE_STAY_TIME 1       /* seconds a traveler stays in a node */

/* Message types sent from child to parent */
#define MSG_WAITING  0   /* about to try entering a node (before sem_wait) */
#define MSG_ENTERED  1   /* acquired semaphore, now inside the node        */
#define MSG_MOVING   2   /* released semaphore, traversing edge             */
#define MSG_FINISHED 3   /* journey complete                                */

typedef struct {
    int type;
    int node;       /* current / from-node */
    int next_node;  /* -1 if destination   */
} TravelerMsg;

typedef enum { T_IDLE, T_WAITING, T_IN_NODE, T_MOVING, T_AT_DEST, T_FINISHED } T6State;

/* Small FIFO message queue per traveler */
#define QSIZE 32
typedef struct { TravelerMsg buf[QSIZE]; int head, tail; } MsgQueue;

static void q_push(MsgQueue *q, TravelerMsg m) {
    q->buf[q->tail % QSIZE] = m;
    q->tail++;
}
static int q_pop(MsgQueue *q, TravelerMsg *m) {
    if (q->head == q->tail) return 0;
    *m = q->buf[q->head++ % QSIZE];
    return 1;
}

typedef struct {
    int      src, dst;
    pid_t    pid;
    Color    color;
    int      read_fd;
    T6State  state;
    int      at_node;
    int      target_node;
    int      edge_weight;
    int      edge_step;
    float    timer;
    Vector2  position;
    MsgQueue queue;
} Traveler6;

static Color palette[]   = {GREEN, ORANGE, PURPLE, YELLOW, PINK, RED, BLUE, BROWN};
static int   palette_size = 8;

/* ------------------------------------------------------------------ */
/* Child: compute own path, traverse with semaphore-guarded node visits */
/* ------------------------------------------------------------------ */
static void child_run(int fd, const Graph *g, int src, int dst, sem_t *sems[]) {
    int path[MAX_NODES], len, w;

    if (!dijkstra_path(g, src, dst, path, &len, &w)) {
        TravelerMsg m = {MSG_FINISHED, -1, -1};
        write(fd, &m, sizeof(m));
        close(fd);
        _exit(0);
    }

    for (int i = 0; i < len; i++) {
        int node = path[i];
        int next = (i < len - 1) ? path[i + 1] : -1;

        /* Notify parent: waiting outside node */
        TravelerMsg m = {MSG_WAITING, node, next};
        write(fd, &m, sizeof(m));

        /* Enter critical section (may block until node is free) */
        sem_wait(sems[node]);

        /* Notify parent: inside node */
        m.type = MSG_ENTERED;
        write(fd, &m, sizeof(m));

        sleep(NODE_STAY_TIME); /* occupy node for 1 second */

        sem_post(sems[node]); /* release node */

        /* Traverse edge to next node */
        if (next != -1) {
            int ew = g->matrix[node][next];
            m = (TravelerMsg){MSG_MOVING, node, next};
            write(fd, &m, sizeof(m));
            usleep((long)ew * (long)(MOVE_STEP_TIME * 1000000));
        }
    }

    TravelerMsg done = {MSG_FINISHED, path[len - 1], -1};
    write(fd, &done, sizeof(done));
    close(fd);
    _exit(0);
}

/* ------------------------------------------------------------------ */
/* Apply a dequeued message to a traveler                               */
/* ------------------------------------------------------------------ */
static void apply_msg(Traveler6 *t, const TravelerMsg *m,
                      const Graph *g, Vector2 pos[]) {
    switch (m->type) {
    case MSG_WAITING:
        t->state = T_WAITING;
        break;

    case MSG_ENTERED:
        printf("[PID=%d] arrived at node %d", (int)t->pid, m->node);
        if (m->next_node == -1) printf(" | DESTINATION");
        else                    printf(" | next node: %d", m->next_node);
        printf("\n");
        fflush(stdout);
        t->at_node   = m->node;
        t->position  = pos[m->node];
        t->state     = (m->next_node == -1) ? T_AT_DEST : T_IN_NODE;
        break;

    case MSG_MOVING:
        t->at_node     = m->node;
        t->target_node = m->next_node;
        t->edge_weight = g->matrix[m->node][m->next_node];
        t->edge_step   = 0;
        t->timer       = 0.0f;
        t->state       = T_MOVING;
        break;

    case MSG_FINISHED:
        printf("[PID=%d] finished\n", (int)t->pid);
        fflush(stdout);
        t->state = T_FINISHED;
        break;
    }
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */
int main(int argc, char *argv[]) {
    if (argc < 2) { printf("Usage: ./sim <file>\n"); return 1; }

    Graph graph;
    TravelerSpec specs[MAX_TRAVELERS];
    int nt = 0;

    if (!load_graph_with_travelers(argv[1], &graph, specs, &nt))
        return 1;

    /* Create one semaphore per node (initial value = 1, i.e. one slot) */
    sem_t *sems[MAX_NODES];
    for (int i = 0; i < graph.nodes; i++) {
        char name[32];
        snprintf(name, sizeof(name), "/gs_node_%d", i);
        sem_unlink(name); /* remove any stale semaphore */
        sems[i] = sem_open(name, O_CREAT | O_EXCL, 0644, 1);
        if (sems[i] == SEM_FAILED) { perror("sem_open"); return 1; }
    }

    /* Create pipes */
    int pfds[MAX_TRAVELERS][2];
    for (int i = 0; i < nt; i++) {
        if (pipe(pfds[i]) < 0) { perror("pipe"); return 1; }
    }

    /* Fork children */
    Traveler6 travelers[MAX_TRAVELERS];

    for (int i = 0; i < nt; i++) {
        Traveler6 *t  = &travelers[i];
        t->src        = specs[i].src;
        t->dst        = specs[i].dst;
        t->color      = palette[i % palette_size];
        t->state      = T_IDLE;
        t->at_node    = specs[i].src;
        t->edge_step  = 0;
        t->timer      = 0.0f;
        t->queue.head = t->queue.tail = 0;

        pid_t pid = fork();
        if (pid == 0) {
            for (int j = 0; j < nt; j++) {
                close(pfds[j][0]);
                if (j != i) close(pfds[j][1]);
            }
            child_run(pfds[i][1], &graph, specs[i].src, specs[i].dst, sems);
        }
        if (pid < 0) { perror("fork"); return 1; }
        t->pid    = pid;
        t->read_fd = pfds[i][0];
        close(pfds[i][1]);
        fcntl(t->read_fd, F_SETFL, O_NONBLOCK);
    }

    InitWindow(WIDTH, HEIGHT, "Graph Simulation - Milestone 6");
    SetTargetFPS(60);

    Vector2 positions[MAX_NODES];
    init_node_positions(&graph, positions);
    for (int i = 0; i < nt; i++)
        travelers[i].position = positions[travelers[i].src];

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        /* 1. Drain pipes into per-traveler queues */
        for (int i = 0; i < nt; i++) {
            Traveler6 *t = &travelers[i];
            if (t->state == T_FINISHED) continue;
            TravelerMsg msg;
            while (read(t->read_fd, &msg, sizeof(msg)) == (ssize_t)sizeof(msg))
                q_push(&t->queue, msg);
        }

        /* 2. Advance edge animations */
        for (int i = 0; i < nt; i++) {
            Traveler6 *t = &travelers[i];
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
                    t->state    = T_IDLE; /* ready to process next messages */
                }
            }
        }

        /* 3. Process queued messages (not while animating) */
        for (int i = 0; i < nt; i++) {
            Traveler6 *t = &travelers[i];
            if (t->state == T_MOVING || t->state == T_FINISHED) continue;

            TravelerMsg msg;
            while (q_pop(&t->queue, &msg)) {
                apply_msg(t, &msg, &graph, positions);
                if (t->state == T_MOVING) break; /* new animation started */
            }
        }

        /* 4. Draw */
        BeginDrawing();
        ClearBackground(RAYWHITE);
        draw_graph(&graph, positions);

        for (int i = 0; i < nt; i++) {
            Traveler6 *t = &travelers[i];
            Color c = (t->state == T_WAITING) ? GRAY : t->color;
            DrawCircleV(t->position, 12, c);
            DrawCircleLines((int)t->position.x, (int)t->position.y, 12, BLACK);
            char lbl[16];
            sprintf(lbl, "%d", i);
            DrawText(lbl, (int)t->position.x - 5, (int)t->position.y - 8, 18, BLACK);

            if (t->state == T_WAITING)
                DrawText("W", (int)t->position.x - 4,
                         (int)t->position.y - 28, 16, RED);
        }

        EndDrawing();
    }

    CloseWindow();

    /* Cleanup semaphores */
    for (int i = 0; i < graph.nodes; i++) {
        char name[32];
        snprintf(name, sizeof(name), "/gs_node_%d", i);
        sem_close(sems[i]);
        sem_unlink(name);
    }

    /* Reap children */
    for (int i = 0; i < nt; i++) {
        close(travelers[i].read_fd);
        waitpid(travelers[i].pid, NULL, 0);
    }

    return 0;
}
