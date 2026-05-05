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

## Notes

* Invalid input prints: `Invalid input`
* If no path exists: `No path found`
* Works on Linux environment
* Uses raylib for graphical interface
