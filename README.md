# Graph Simulation Project

## Team Members

* Ahmad Abu Gosh
* Wajdi Elfrawna
* Saleem Naamna

---

## Project Description

This project simulates movement on a directed weighted graph.

It includes:

* Loading a graph from a file
* Computing the shortest path using Dijkstra algorithm
* Visualizing the graph using raylib (GUI)
* Animating movement along the shortest path

---

## Milestone 1 – Dijkstra Algorithm

### Compile

```bash
make milestone1
```

### Run

```bash
./dijkstra inputs/graph1.txt
```

### Description

* Reads a directed weighted graph from a file
* Computes the shortest path using Dijkstra algorithm
* Prints the path and total weight

---

## Milestone 2 – Graph Visualization (GUI)

### Compile

```bash
make milestone2
```

### Run

```bash
./sim inputs/graph1.txt
```

### Description

* Displays the graph using raylib
* Nodes are shown as circles
* Edges are shown as arrows
* Edge weights are displayed
* Graph is arranged in a circular layout for clarity

---

## Milestone 3 – Animation

### Compile

```bash
make milestone3
```

### Run

```bash
./sim inputs/graph1.txt
```

### Description

* Animates an entity moving along the shortest path
* Movement is based on edge weight (300ms per unit)
* Stops for 1 second at intermediate nodes
* Includes Play/Stop button
* Displays the path and total weight
* Shows message when destination is reached

---

---

## Milestone 4 – Multiple Travelers (Parent/Child Processes)

### Compile

```bash
make milestone4
```

### Run

```bash
./sim inputs/graph_m4.txt
```

### Input file format

```
# graph definition
5 7
0 1 4
...
# travelers
2
0 4
2 3
```

### Description

* Reads multiple travelers from an extended input file
* Parent process computes the Dijkstra path for every traveler
* Parent forks one child per traveler using `fork()`; each child prints `[PID] started` then sleeps
* Parent manages the raylib GUI loop and animates all travelers simultaneously, each in a different color
* When a traveler reaches its destination the parent sends `SIGTERM` to the matching child
* Parent waits for all children before exiting

---

## Milestone 5 – IPC Between Processes (Pipes)

### Compile

```bash
make milestone5
```

### Run

```bash
./sim inputs/graph_m4.txt
```

### IPC mechanism chosen: anonymous pipes (`pipe()`)

Pipes were chosen because they are simple, well-supported, and provide a natural unidirectional channel from each child to the parent. One pipe is created per traveler before `fork()`. After forking, each child closes all read ends and all write ends that do not belong to it; the parent closes all write ends and sets all read ends to non-blocking (`O_NONBLOCK`) so the raylib game loop never stalls.

### Description

* Parent does **not** compute paths – path data never flows from parent to child
* Each child independently computes its own Dijkstra path and traverses it
* On each node arrival the child sends a fixed-size `TravelerMsg` struct to the parent
* The parent reads messages non-blocking in the game loop, updates the GUI, and prints:
  * `[PID=X] arrived at node N | next node: M`
  * `[PID=X] arrived at node N | DESTINATION`
  * `[PID=X] finished`
* Only the parent prints to the terminal

### Example output

```
[PID=1021] arrived at node 0 | next node: 2
[PID=1022] arrived at node 2 | next node: 1
[PID=1021] arrived at node 2 | next node: 1
[PID=1022] arrived at node 1 | next node: 3
[PID=1021] arrived at node 1 | next node: 4
[PID=1022] arrived at node 3 | DESTINATION
[PID=1021] arrived at node 4 | DESTINATION
[PID=1022] finished
[PID=1021] finished
```

---

---

## Milestone 6 – Node Access Synchronization

### Compile

```bash
make milestone6
```

### Run

```bash
./sim inputs/graph_m6.txt
```

### Synchronization mechanism: POSIX Named Semaphores (`sem_open`)

One binary semaphore is created per node before forking (`/gs_node_N`). When a child wants to enter a node it calls `sem_wait()` (which may block if another traveler is already inside), stays for exactly 1 second, then calls `sem_post()` to release the node.

This guarantees mutual exclusion per node with no starvation (POSIX semaphores are fair on Linux).

### IPC mechanism: Anonymous pipes (same as Milestone 5)

### Description

* At most one traveler may occupy a node at any moment
* Travelers waiting outside a node are shown in **gray** with a **W** indicator
* Travelers inside a node are shown in their assigned color
* The parent receives `MSG_WAITING`, `MSG_ENTERED`, `MSG_MOVING`, and `MSG_FINISHED` messages from children and updates the GUI accordingly
* Children compute their own Dijkstra paths (no path data from parent)

---

---

## Milestone 7 – Scheduling Algorithms

### Compile

```bash
make milestone7
```

### Run

```bash
./sim -schd fcfs inputs/graph_m7.txt
./sim -schd sjf  inputs/graph_m7.txt
```

### Scheduling algorithms implemented

**FCFS – First Come First Served**
When multiple travelers wait to enter the same node, they are admitted in the order their `MSG_WAITING` message reached the parent (i.e., arrival order). The parent assigns each waiter a monotonically increasing sequence number and always wakes the traveler with the smallest number next.

**SJF – Shortest Job First**
Each child computes the total remaining path weight from the current node to its destination and sends it as a priority value in the `MSG_WAITING` message. The parent always wakes the waiter with the smallest remaining weight next. Travelers with a shorter onward journey are served first, reducing their average waiting time.

### How it works

* Children no longer call `sem_wait` directly. Instead each child sends `MSG_WAITING` and blocks on a per-child signal pipe until the parent grants entry.
* The parent maintains a waiting queue per node. On receiving `MSG_WAITING` it inserts the traveler with its priority and calls `schedule_next`, which wakes the best candidate immediately if the node is free.
* When a traveler finishes occupying a node it sends `MSG_LEAVING`; the parent marks the node free and calls `schedule_next` again to admit the next waiter.
* Two pipes per traveler: one **child→parent** data pipe (messages) and one **parent→child** signal pipe (1-byte grant).

### GUI differences

* A coloured banner in the top-left corner shows the active algorithm (**blue** for FCFS, **green** for SJF).
* Each node with queued waiters shows a **Q:N** counter.
* Waiting travelers are drawn in **gray** with a **W** label.
* When a traveler finishes, the terminal prints total accumulated wait time and the number of node-waits.

### Algorithm comparison (same input `graph_m7.txt`)

The test graph routes 4 travelers through a single bottleneck (node 2). Two have a short onward path (weight 1) and two have a long onward path (weight 5).

| Algorithm | Ordering at node 2 | Effect |
|---|---|---|
| FCFS | Arrival order (non-deterministic) | Short and long jobs interleaved; long jobs may block short ones |
| SJF | Shortest remaining path first (1 before 5) | Short-path travelers always admitted first; their total wait is minimized |

With SJF the two travelers heading to nodes 3 and 4 (remaining weight = 1) are always served before the two heading to nodes 5 and 6 (remaining weight = 5), leading to lower average wait time for those travelers.

---

## Notes

* Invalid input prints: `Invalid input`
* If no path exists: `No path found`
* Works on Linux environment
* Uses raylib for graphical interface
