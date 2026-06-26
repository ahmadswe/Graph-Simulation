/*
 * ===================================================================
 * MILESTONE 5 - צינורות (Pipes)
 * ===================================================================
 * מה קורה כאן:
 *   - כל נוסע מקבל תהליך בן + pipe תקשורת (כיוון: בן -> אב)
 *   - הבן מחשב את מסלול דייקסטרה ושולח הודעות לאב דרך ה-pipe
 *   - האב קורא הודעות (non-blocking) ומעדכן אנימציה
 *
 * מבנה ה-pipe:
 *   pipefds[i][0] = קצה קריאה (אצל האב)
 *   pipefds[i][1] = קצה כתיבה (אצל הבן)
 *
 * נקודות שינוי שכיחות בבחינה:
 *   1. הוספת שדה ל-TravelerMsg -> עדכן גם את השליחה ב-run_child
 *   2. שינוי ה-sleep בבן -> usleep((long)w * ...)  שורה ~71
 *   3. הוספת הדפסה ב-apply_message -> שורה ~84
 *   4. שינוי מה קורה כשמגיעים ליעד -> שורה ~93
 * ===================================================================
 */

#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include "raylib.h"
#include "graph.h"
#include "dijkstra.h"
#include "draw_utils.h"

#define MOVE_STEP_TIME 0.3f

/* *** הודעה שהבן שולח לאב דרך ה-pipe ***
 * לשינוי פרוטוקול: הוסף שדות כאן ועדכן את run_child ו-apply_message
 */
typedef struct {
    int current_node;
    int next_node;  /* -1 = reached destination */
    int finished;   /* 1 = child is exiting */
    int no_path;    /* 1 = no path exists to destination (graph not connected) */
} TravelerMsg;

typedef enum {
    T_AT_NODE,
    T_MOVING,
    T_AT_DEST,   /* arrived at destination, waiting for "finished" msg */
    T_FINISHED
} TravelerState;

typedef struct {
    int          src, dst;
    pid_t        pid;
    Color        color;
    int          read_fd;

    TravelerState state;
    int           current_node;
    int           target_node;
    int           edge_weight;
    int           edge_step;
    float         timer;
    Vector2       position;

    /* one-slot buffer for messages arriving while still animating */
    TravelerMsg   pending;
    int           has_pending;
} Traveler;

static Color palette[]   = {GREEN, ORANGE, PURPLE, YELLOW, PINK, RED, BLUE, BROWN};
static int   palette_size = 8;

/* ------------------------------------------------------------------ */
/* *** קוד תהליך הבן ***
 * הבן מחשב מסלול, ואז לכל צומת:
 *   1. שולח הודעה עם הצומת הנוכחי והבא
 *   2. מחכה (usleep) לפי משקל הקשת
 * בסוף שולח הודעת finished ויוצא
 * ------------------------------------------------------------------ */
static void run_child(int write_fd, const Graph *graph, int src, int dst) {
    int path[MAX_NODES], path_length, total_weight;

    if (!dijkstra_path(graph, src, dst, path, &path_length, &total_weight)) {
        /* *** הודעה מיוחדת: אין מסלול ליעד (גרף לא קשיר) *** */
        TravelerMsg no_path_msg = {-1, -1, 0, 1};  /* no_path=1, finished=0 */
        write(write_fd, &no_path_msg, sizeof(no_path_msg));
        close(write_fd);
        _exit(0);
    }

    for (int i = 0; i < path_length; i++) {
        TravelerMsg msg;
        msg.finished     = 0;
        msg.current_node = path[i];
        msg.next_node    = (i < path_length - 1) ? path[i + 1] : -1;
        msg.no_path      = 0;
        /* *** שליחת הודעה לאב דרך ה-pipe *** */
        write(write_fd, &msg, sizeof(msg));

        if (i < path_length - 1) {
            int w = graph->matrix[path[i]][path[i + 1]];
            /* *** המתנה לפי משקל הקשת - לשינוי זמן ההמתנה שנה כאן *** */
            usleep((long)w * (long)(MOVE_STEP_TIME * 1000000));
        }
    }

    /* *** הודעת סיום - finished=1 *** */
    TravelerMsg done = {-1, -1, 1, 0};
    write(write_fd, &done, sizeof(done));
    close(write_fd);
    _exit(0);
}

/* ------------------------------------------------------------------ */
/* *** עיבוד הודעה שהתקבלה מהבן - עדכון מצב הנוסע ***
 * שינויים שכיחים: הוסף הדפסות, שנה התנהגות בהגעה ליעד
 * ------------------------------------------------------------------ */
static void apply_message(Traveler *t, const TravelerMsg *msg,
                           const Graph *graph, Vector2 positions[]) {
    /* *** טיפול בהודעת "אין מסלול" - נפרד מהודעות רגילות *** */
    if (msg->no_path) {
        printf("[PID=%d] NO PATH from node %d to node %d (graph not connected)\n",
               (int)t->pid, t->src, t->dst);
        fflush(stdout);
        t->state = T_FINISHED;
        return;
    }

    if (msg->finished) {
        printf("[PID=%d] finished\n", (int)t->pid);
        fflush(stdout);
        t->state = T_FINISHED;
        return;
    }

    if (msg->next_node == -1) {
        printf("[PID=%d] arrived at node %d | DESTINATION\n",
               (int)t->pid, msg->current_node);
        fflush(stdout);
        t->position = positions[msg->current_node];
        t->state    = T_AT_DEST;   /* keep pipe open to receive "finished" */
        return;
    }

    printf("[PID=%d] arrived at node %d | next node: %d\n",
           (int)t->pid, msg->current_node, msg->next_node);
    fflush(stdout);

    t->current_node = msg->current_node;
    t->target_node  = msg->next_node;
    t->edge_weight  = graph->matrix[msg->current_node][msg->next_node];
    t->edge_step    = 0;
    t->timer        = 0.0f;
    t->position     = positions[msg->current_node];
    t->state        = T_MOVING;
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */
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

    /* *** יצירת pipe לכל נוסע לפני ה-fork ***
     * pipefds[i][0] = read end  (האב קורא מכאן)
     * pipefds[i][1] = write end (הבן כותב לכאן)
     */
    int pipefds[MAX_TRAVELERS][2];
    for (int i = 0; i < num_travelers; i++) {
        if (pipe(pipefds[i]) < 0) {
            perror("pipe");
            return 1;
        }
    }

    Traveler travelers[MAX_TRAVELERS];

    for (int i = 0; i < num_travelers; i++) {
        Traveler *t  = &travelers[i];
        t->src        = specs[i].src;
        t->dst        = specs[i].dst;
        t->color      = palette[i % palette_size];
        t->state      = T_AT_NODE;
        t->has_pending = 0;
        t->current_node = specs[i].src;
        t->target_node  = -1;
        t->edge_step    = 0;
        t->timer        = 0.0f;

        pid_t pid = fork();
        if (pid == 0) {
            /* Child: close all read ends and all other write ends */
            for (int j = 0; j < num_travelers; j++) {
                close(pipefds[j][0]);
                if (j != i) close(pipefds[j][1]);
            }
            run_child(pipefds[i][1], &graph, specs[i].src, specs[i].dst);
            /* unreachable */
        }
        if (pid < 0) {
            perror("fork");
            return 1;
        }
        t->pid     = pid;
        t->read_fd = pipefds[i][0];

        /* Parent closes the write end for this child's pipe */
        close(pipefds[i][1]);

        /* *** non-blocking read - האב לא נחסם אם אין הודעה ***
         * בלי זה, read() היה חוסם את לולאת הציור
         */
        fcntl(t->read_fd, F_SETFL, O_NONBLOCK);
    }

    InitWindow(WIDTH, HEIGHT, "Graph Simulation - Milestone 5");
    SetTargetFPS(60);

    Vector2 positions[MAX_NODES];
    init_node_positions(&graph, positions);

    /* Initialise visual positions to each traveler's source node */
    for (int i = 0; i < num_travelers; i++)
        travelers[i].position = positions[travelers[i].src];

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        /* --- Read messages from children ----------------------------- */
        for (int i = 0; i < num_travelers; i++) {
            Traveler *t = &travelers[i];
            if (t->state == T_FINISHED) continue;
            /* T_AT_DEST: still read pipe to catch the "finished" message */

            TravelerMsg msg;
            ssize_t n = read(t->read_fd, &msg, sizeof(msg));
            if (n == (ssize_t)sizeof(msg)) {
                if (t->state == T_MOVING && !msg.finished && !msg.no_path && msg.next_node != -1) {
                    /* Still animating — buffer the message */
                    t->pending     = msg;
                    t->has_pending = 1;
                } else {
                    apply_message(t, &msg, &graph, positions);
                }
            }
        }

        /* --- Advance animations -------------------------------------- */
        for (int i = 0; i < num_travelers; i++) {
            Traveler *t = &travelers[i];
            if (t->state != T_MOVING) continue;

            t->timer += dt;
            if (t->timer >= MOVE_STEP_TIME) {
                t->timer = 0.0f;
                t->edge_step++;

                float p = (float)t->edge_step / t->edge_weight;
                t->position.x = positions[t->current_node].x +
                    (positions[t->target_node].x - positions[t->current_node].x) * p;
                t->position.y = positions[t->current_node].y +
                    (positions[t->target_node].y - positions[t->current_node].y) * p;

                if (t->edge_step >= t->edge_weight) {
                    t->position = positions[t->target_node];
                    t->state    = T_AT_NODE;

                    if (t->has_pending) {
                        apply_message(t, &t->pending, &graph, positions);
                        t->has_pending = 0;
                    }
                }
            }
        }

        /* --- Draw ---------------------------------------------------- */
        BeginDrawing();
        ClearBackground(RAYWHITE);

        draw_graph(&graph, positions);

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

    /* Reap all children */
    for (int i = 0; i < num_travelers; i++) {
        close(travelers[i].read_fd);
        waitpid(travelers[i].pid, NULL, 0);
    }

    return 0;
}