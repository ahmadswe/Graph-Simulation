#include <stdio.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: ./dijkstra <file>\n");
        return 1;
    }

    printf("Reading file: %s\n", argv[1]);
    return 0;
}