CC = gcc
CFLAGS = -Wall -Wextra -g

milestone1:
	$(CC) $(CFLAGS) src/main.c src/graph.c src/dijkstra.c -o dijkstra

milestone2:
	$(CC) $(CFLAGS) src/sim_main.c src/graph.c src/dijkstra.c -o sim -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
	
milestone3:
	$(CC) $(CFLAGS) src/sim_main.c src/graph.c src/dijkstra.c -o sim -lraylib -lGL -lm -lpthread -ldl -lrt -lX11	

clean:
	rm -f dijkstra sim