CC = gcc
CFLAGS = -Wall -Wextra -g

milestone1:
	$(CC) $(CFLAGS) src/main.c src/graph.c src/dijkstra.c -o dijkstra

clean:
	rm -f dijkstra sim