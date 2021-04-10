#include<linux/module.h>
#include<linux/kernel.h>
#include<linux/init.h>
#include<linux/sched.h>
#include<linux/unistd.h>
#include "ptree.h"
MODULE_LICENSE("Dual BSD/GPL");
#define __NR_ptree 356

/*control if output debug information, if defined with DEBUG, all output information between ifdef DEBUG and endif can been seen*/
// #define DEBUG

static int (*oldcall)(void);

// use dfs to traverse all the task.
static void dfs(struct prinfo *buf, int *nr, int nr_max, struct task_struct *task, int dep){
#ifdef DEBUG
    printk("in dfs of level %d\n", dep);
    printk("nr:%d maxnr:%d pid:%d\n", *nr, nr_max, task->pid);
#endif
    if (*nr > nr_max)
        return;

    //copy target information from task to buf
    if (task->real_parent == NULL)
        buf[*nr].parent_pid = 0;
    else
        buf[*nr].parent_pid = task->real_parent->pid;
    buf[*nr].pid = task->pid;
    if (list_empty(&task->children))
        buf[*nr].first_child_pid = 0;
    else{
        buf[*nr].first_child_pid = list_entry(task->children.next, struct task_struct, sibling)->pid;
#ifdef DEBUG
        struct list_head *child;
		printk("children: ");
		list_for_each(child, &task->children)
			printk("%d ", list_entry(child, struct task_struct, sibling)->pid);
		printk("\n");
#endif
    }
    if (list_empty(&task->sibling))
        buf[*nr].next_sibling_pid = 0;
    else{
        buf[*nr].next_sibling_pid = list_entry(task->sibling.next, struct task_struct, sibling)->pid;
#ifdef DEBUG
        struct list_head *sibling;
		printk("siblings: ");
		list_for_each(sibling, &task->sibling)
			printk("%d ", list_entry(sibling, struct task_struct, sibling)->pid);
		printk("\n");
#endif
    }
    buf[*nr].state = task->state;
    buf[*nr].uid = task->cred->uid;
    buf[*nr].depth = dep;
    strcpy(buf[*nr].comm, task->comm);
#ifdef DEBUG
    int i=0;
    for (i = 0; i < dep; ++i)
        printk("-");
    printk("%s,%d,%ld,%d,%d,%d,%d\n", buf[*nr].comm, buf[*nr].pid, buf[*nr].state, buf[*nr].parent_pid, buf[*nr].first_child_pid, buf[*nr].next_sibling_pid, buf[*nr].uid);
#endif

    *nr += 1; //increase counter

    struct list_head *child;
    /* traverse the child and they belong to next level.
     * list for each is implanted in list.h*/
    list_for_each(child, &task->children)
        dfs(buf, nr, nr_max, list_entry(child, struct task_struct, sibling), dep + 1);
}

static int ptree(struct prinfo *buf, int *nr)
{
    // deal with exceptions
    if (buf == NULL){
        printk("argument pointer is null\n");
        return PTREE_ERROR_POINTER_NULL;
    }

    if (*nr <= 0){
        printk("nr is less than zero\n");
        return PTREE_ERROR_NR_LESS_THAN_ZERO;
    }

    int nr_old = *nr;
    *nr = 0;

#ifdef DEBUG
    printk("in systemcall ptree, nr = %d, buf:%p\n", *nr, buf);
#endif

    //get lock and do the dfs
    read_lock(&tasklist_lock);
#ifdef DEBUG
    printk("read lock successfully!\n");
#endif
    dfs(buf, nr, nr_old, &init_task, 0);
#ifdef DEBUG
    printk("dfs done\n");
#endif
    read_unlock(&tasklist_lock);

    return 0;
}

static int addsyscall_init(void)
{
    long *syscall = (long*)0xc000d8c4;
    oldcall = (int(*)(void))(syscall[__NR_ptree]);
    syscall[__NR_ptree] = (unsigned long )ptree;
    printk(KERN_INFO "ptree module load!\n");
    return 0;
}

static void addsyscall_exit(void)
{
    long *syscall = (long*)0xc000d8c4;
    syscall[__NR_ptree] = (unsigned long)oldcall;
    printk(KERN_INFO "ptree module exit!\n");
}

module_init(addsyscall_init);
module_exit(addsyscall_exit);