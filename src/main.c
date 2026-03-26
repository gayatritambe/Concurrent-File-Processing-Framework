#include <stdio.h>
#include "controller.h"
int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <instructions.txt> <folder>\n", argv[0]);
        return 1;
    }
    start_controller(argc, argv);
    printf("Instructions File: %s\n", argv[1]);
    printf("Target Folder: %s\n", argv[2]);

    return 0;
}
