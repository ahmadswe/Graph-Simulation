CC = gcc
CFLAGS = -Wall -Wextra -g

milestone1:
	$(CC) $(CFLAGS) src/milestone1_main.c -o dijkstra

clean:
	rm -f dijkstra