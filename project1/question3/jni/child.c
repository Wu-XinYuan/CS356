#include<stdlib.h>
#include <sys/syscall.h>
#include <stdio.h>
#include "../../module/ptree.h"
#define __NR_ptree 356

/* a simple code of use fork()
 * shows the pid of parent and child pid*/
int main(){
    pid_t pid = fork();
    if(pid < 0){ //deal with exceptions
        printf("Failed to fork.\n");
        return 1;
    }
    else if(pid == 0){ //pid=0 means it is child process
        pid_t pid_c = getpid();
        printf("519021910604 Child pid: %d\n", pid_c);
        execl("/data/misc/mymodule/testARM", "ptree", NULL);
    }
    else{ //in parent process
        pid_t pid_p = getpid();
        printf("519021910604 Parent pid:%d\n", pid_p);
    }

    return 0;
}
