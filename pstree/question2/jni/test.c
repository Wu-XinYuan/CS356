#include <stdio.h>
#include <stdlib.h>
#include "../../module/ptree.h"
#define BUFFER_SIZE 1024

struct prinfo buf[BUFFER_SIZE];
int nr = BUFFER_SIZE;

/* a simple code to test the system call written in ptree.c
 * no input argument needed*/
int main(int argc, char *argv[]) {
    int res = syscall(356, buf, &nr);
    // if system call went wrong, print error message
    if (res!=0){
        printf("ERROR: system call ptree error %d\n", res);
        return res;
    }

    // after system call, information already put in order in buffer. Just need to traverse buffer
    int i, j;
    for (i = 0; i < nr; ++i){
        for (j = 0; j < buf[i].depth; j++)
            printf("\t");
        printf("%s,%d,%ld,%d,%d,%d,%d\n", buf[i].comm, buf[i].pid, buf[i].state, buf[i].parent_pid, buf[i].first_child_pid, buf[i].next_sibling_pid, buf[i].uid);
    }
}
