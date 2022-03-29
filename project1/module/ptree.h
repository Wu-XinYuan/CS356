#ifndef PSTREEHEADER_H_
#define PSTREEHEADER_H_

/* define error may happen in ptree*/
#define PTREE_ERROR_POINTER_NULL -1  /* buf or nr is null*/
#define PTREE_ERROR_NR_LESS_THAN_ZERO -2 /* ptree argument nr<=0 */

struct prinfo {
    pid_t parent_pid; /* process id of parent */
    pid_t pid; /* process id */
    pid_t first_child_pid; /* pid of youngest child */
    pid_t next_sibling_pid; /* pid of older sibling */
    long state; /* current state of process */
    long uid; /* user id of process owner */
    int depth; /*newly added, for the convenience of print*/
    char comm[16]; /* name of program executed, should not be too long like 64, may cause stack corruption*/
};

#endif /*PSTREEHEADER_H_*/