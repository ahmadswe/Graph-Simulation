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

## Notes

* Invalid input prints: `Invalid input`
* If no path exists: `No path found`
* Works on Linux environment
* Uses raylib for graphical interface
