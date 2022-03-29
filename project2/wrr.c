//
// Created by wxy on 2021/6/2.
//


#include "sched.h"
#include <linux/slab.h>

#define RUNTIME_INF	((u64)~0ULL)

struct wrr_bandwidth def_wrr_bandwidth;

void init_wrr_bandwidth(struct wrr_bandwidth *wrr_b, u64 period, u64 runtime)
{
    wrr_b->wrr_period = ns_to_ktime(period);
    wrr_b->wrr_runtime = runtime;

    raw_spin_lock_init(&wrr_b->wrr_runtime_lock);

    hrtimer_init(&wrr_b->wrr_period_timer,
                 CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    wrr_b->wrr_period_timer.function = HRTIMER_RESTART;
}


void init_wrr_rq(struct wrr_rq *wrr_rq)
{
    struct list_head *queue = wrr_rq -> queue;
    INIT_LIST_HEAD(queue);
    wrr_rq->queue_empty = true;

    wrr_rq->wrr_time = 0;
    wrr_rq->wrr_throttled = 0;
    wrr_rq->wrr_runtime = 0;
    raw_spin_lock_init(&wrr_rq->wrr_runtime_lock);
}


void free_wrr_sched_group(struct task_group *tg)
{
}

int alloc_wrr_sched_group(struct task_group *tg, struct task_group *parent)
{
}

/*
 * all the following functions are either a simple wrap of another function and has a name easy to understand
 * or differ when CONFIG_WRR_GROUP_SCHED is defined or not
 */

static inline int on_wrr_rq(struct sched_wrr_entity *wrr_se)
{
    return !list_empty(&wrr_se->run_list);
}

static inline struct task_struct *wrr_task_of(struct sched_wrr_entity *wrr_se)
{
    return container_of(wrr_se, struct task_struct, wrr);
}

#ifdef CONFIG_WRR_GROUP_SCHED

static inline struct wrr_rq *wrr_rq_of_se(struct sched_wrr_entity *wrr_se)
{
    return wrr_se->wrr_rq;
}

static inline struct wrr_rq *group_wrr_rq(struct sched_wrr_entity *wrr_se)
{
    return wrr_se->my_q;
}

static inline u64 sched_wrr_runtime(struct wrr_rq *wrr_rq)
{
	if (!wrr_rq->tg)
		return RUNTIME_INF;

	return wrr_rq->wrr_runtime;
}

#define for_each_sched_wrr_entity(wrr_se) \
	for (; wrr_se; wrr_se = wrr_se->parent)

static inline struct rq *rq_of_wrr_rq(struct wrr_rq *wrr_rq)
{
    return wrr_rq->rq;
}

static inline void list_add_leaf_wrr_rq(struct wrr_rq *wrr_rq)
{
    list_add_rcu(&wrr_rq->leaf_wrr_rq_list,
                 &(rq_of_wrr_rq(wrr_rq)->leaf_wrr_rq_list));
}

void init_tg_wrr_entry(struct task_group *tg, struct wrr_rq *wrr_rq,
                       struct sched_wrr_entity *wrr_se, int cpu,
                       struct sched_wrr_entity *parent)
{
}


#else

#define for_each_sched_wrr_entity(wrr_se) \
	for (; wrr_se; wrr_se = NULL)

static inline struct wrr_rq *group_wrr_rq(struct sched_wrr_entity *wrr_se)
{
    return NULL;
}

static inline struct wrr_rq *wrr_rq_of_se(struct sched_wrr_entity *wrr_se)
{
    struct task_struct *p = wrr_task_of(wrr_se);
    struct rq *rq = task_rq(p);

    return &rq->wrr;
}

static inline u64 sched_wrr_runtime(struct wrr_rq *wrr_rq)
{
    return wrr_rq->wrr_runtime;
}

static inline void list_add_leaf_wrr_rq(struct wrr_rq *wrr_rq)
{
}

static inline struct rq *rq_of_wrr_rq(struct wrr_rq *wrr_rq)
{
    return container_of(wrr_rq, struct rq, wrr);
}

#endif

static int sched_wrr_runtime_exceeded(struct wrr_rq *wrr_rq)
{
    u64 runtime = sched_wrr_runtime(wrr_rq);
    if (runtime >= sched_wrr_period(wrr_rq))
        return 0;

    balance_runtime(wrr_rq);
    runtime = sched_wrr_runtime(wrr_rq);
    if (runtime == RUNTIME_INF)
        return 0;

    if (wrr_rq->wrr_time > runtime) {
        struct wrr_bandwidth *wrr_b = sched_wrr_bandwidth(wrr_rq);

        if (likely(wrr_b->wrr_runtime)) {
            static bool once = false;

            wrr_rq->wrr_throttled = 1;

            if (!once) {
                once = true;
                printk_sched("sched: WRR throttling activated\n");
            }
        } else
            wrr_rq->wrr_time = 0;

        if (wrr_rq_throttled(wrr_rq)) {
            sched_wrr_rq_dequeue(wrr_rq);
            return 1;
        }
    }
    return 0;
}

/*
 * update the current wrr_entity, also which task is running
 * if use up its time slice, let the next task run
 */
static void update_curr_wrr(struct rq *rq)
{
    printk("updating wrr");
    struct task_struct *curr = rq->curr;
    struct sched_wrr_entity *wrr_se = &curr->wrr;
    struct wrr_rq *wrr_rq = wrr_rq_of_se(wrr_se);
    u64 delta_exec;

    if (curr->sched_class != &wrr_sched_class)
        return;

    delta_exec = rq->clock_task - curr->se.exec_start;
    if (unlikely((s64)delta_exec < 0))
        delta_exec = 0;

    schedstat_set(curr->se.statistics.exec_max,
                  max(curr->se.statistics.exec_max, delta_exec));

    curr->se.sum_exec_runtime += delta_exec;
    account_group_exec_runtime(curr, delta_exec);

    curr->se.exec_start = rq->clock_task;
    cpuacct_charge(curr, delta_exec);

    for_each_sched_wrr_entity(wrr_se) {
        wrr_rq = wrr_rq_of_se(wrr_se);

        if (sched_wrr_runtime(wrr_rq) != RUNTIME_INF) {
            raw_spin_lock(&wrr_rq->wrr_runtime_lock);
            wrr_rq->wrr_time += delta_exec;
            if (sched_wrr_runtime_exceeded(wrr_rq))
                resched_task(curr);
            raw_spin_unlock(&wrr_rq->wrr_runtime_lock);
        }
    }
}

static void __dequeue_wrr_entity(struct sched_wrr_entity *wrr_se)
{
    struct wrr_rq *wrr_rq = wrr_rq_of_se(wrr_se);
    struct list_head *queue = wrr_rq->queue;

    list_del_init(&wrr_se->run_list);
    if (list_empty(queue))
        wrr_rq->queue_empty = false;
    wrr_rq->wrr_nr_running--;
}

static void dequeue_wrr_stack(struct sched_wrr_entity *wrr_se)
{
    struct sched_wrr_entity *back = NULL;

    for_each_sched_wrr_entity(wrr_se) {
        wrr_se->back = back;
        back = wrr_se;
    }

    for (wrr_se = back; wrr_se; wrr_se = wrr_se->back) {
        if (on_wrr_rq(wrr_se))
            __dequeue_wrr_entity(wrr_se);
    }
}

/*
 * put the given entity into queue
 * if head is true, the entity will be put into head
 * else, it will be put into tail
 */
static void __enqueue_wrr_entity(struct sched_wrr_entity *wrr_se, bool head)
{
    struct wrr_rq *wrr_rq = wrr_rq_of_se(wrr_se);
    struct wrr_rq *group_rq = group_wrr_rq(wrr_se);
    struct list_head *queue = wrr_rq->queue;

    if (!wrr_rq->wrr_nr_running)
        list_add_leaf_wrr_rq(wrr_rq);
    if (head)
        list_add(&wrr_se->run_list, queue);
    else
        list_add_tail(&wrr_se->run_list, queue);
    wrr_rq->wrr_nr_running++;
}

/*
 * a wrap of __enqueue_wrr_entity
 */
static void enqueue_wrr_entity(struct sched_wrr_entity *wrr_se, bool head)
{
    printk("enqueue entity");
    dequeue_wrr_stack(wrr_se);
    for_each_sched_wrr_entity(wrr_se)
        __enqueue_wrr_entity(wrr_se, head);
}

/*
 * get the given entity out of queue
 */
static void dequeue_wrr_entity(struct sched_wrr_entity *wrr_se)
{
    printk("dequeue entity");
    dequeue_wrr_stack(wrr_se);
    for_each_sched_wrr_entity(wrr_se) {
        struct wrr_rq *wrr_rq = group_wrr_rq(wrr_se);

        if (wrr_rq && wrr_rq->wrr_nr_running)
            __enqueue_wrr_entity(wrr_se, false);
    }
}

/*
 * put the current task into running queue
 * call enqueue_wrr_entity
 */
static void enqueue_task_wrr(struct rq *rq, struct task_struct *p, int flags)
{
    struct sched_wrr_entity *wrr_se = &p->wrr;
    if (flags & ENQUEUE_WAKEUP)
        wrr_se->timeout = 0;
    enqueue_wrr_entity(wrr_se, flags & ENQUEUE_HEAD);
    inc_nr_running(rq);
}

static void dequeue_task_wrr(struct rq *rq, struct task_struct *p, int flags)
{
    struct sched_wrr_entity *wrr_se = &p->wrr;
    update_curr_wrr(rq);
    dequeue_wrr_entity(wrr_se);
    dec_nr_running(rq);
}

/*
 * Put task to the head or the end of the run list without the overhead of
 * dequeue followed by enqueue.
 */
static void requeue_wrr_entity(struct wrr_rq *wrr_rq, struct sched_wrr_entity *wrr_se, int head)
{
    if (on_wrr_rq(wrr_se)) {
        struct list_head *queue = wrr_rq->queue;
        if (head)
            list_move(&wrr_se->run_list, queue);
        else
            list_move_tail(&wrr_se->run_list, queue);
    }
}

static void requeue_task_wrr(struct rq *rq, struct task_struct *p, int head)
{
    struct sched_wrr_entity *wrr_se = &p->wrr;
    struct wrr_rq *wrr_rq;

    for_each_sched_wrr_entity(wrr_se) {
        wrr_rq = wrr_rq_of_se(wrr_se);
        requeue_wrr_entity(wrr_rq, wrr_se, head);
    }
}

static void yield_task_wrr(struct rq *rq)
{
    //remove the current task to tail
    requeue_task_wrr(rq, rq->curr, 0);
}

static void check_preempt_curr_wrr(struct rq *rq, struct task_struct *p, int flags)
{
}

static struct sched_wrr_entity *pick_next_wrr_entity(struct rq *rq, struct wrr_rq *wrr_rq)
{
    struct sched_wrr_entity *next = NULL;
    struct list_head *queue = wrr_rq->queue;
    next = list_entry(queue->next, struct sched_wrr_entity, run_list);
    return next;
}

static struct task_struct *_pick_next_task_wrr(struct rq *rq)
{
    struct sched_wrr_entity *wrr_se;
    struct task_struct *p;
    struct wrr_rq *wrr_rq;

    wrr_rq = &rq->wrr;

    /*rq is empty, then end*/
    if (!wrr_rq->wrr_nr_running)
        return NULL;
    do {
        wrr_se = pick_next_wrr_entity(rq, wrr_rq);
        BUG_ON(!wrr_se);
        wrr_rq = group_wrr_rq(wrr_se);
    } while (wrr_rq);
    p = wrr_task_of(wrr_se);
    p->se.exec_start = rq->clock_task;
    return p;
}

/*
 * pick the next task from queue
 */
static struct task_struct *pick_next_task_wrr(struct rq *rq)
{
    struct task_struct *p = _pick_next_task_wrr(rq);
    return p;
}


static void put_prev_task_wrr(struct rq *rq, struct task_struct *p)
{
    update_curr_wrr(rq);
}

static int select_task_rq_wrr(struct task_struct *p, int sd_flag, int flags) {
}

static void set_cpus_allowed_wrr(struct task_struct *p, const struct cpumask *new_mask)
{
}

static void rq_online_wrr(struct rq *rq)
{
}

static void rq_offline_wrr(struct rq *rq)
{
}

static void pre_schedule_wrr(struct rq *rq, struct task_struct *prev)
{
}

static void post_schedule_wrr(struct rq *rq)
{
}

static void task_woken_wrr(struct rq *rq, struct task_struct *p)
{
}

static void switched_from_wrr(struct rq *rq, struct task_struct *p)
{
}

static void set_curr_task_wrr(struct rq *rq)
{
    struct task_struct *p = rq->curr;
    p->se.exec_start = rq->clock_task;
}

/*
 * decide whether to turn to a next task assign the timeslice
 */
static void task_tick_wrr(struct rq *rq, struct task_struct *p, int queued)
{
    struct sched_wrr_entity *wrr_se = &p->wrr;
    update_curr_wrr(rq);
    if (p->policy != SCHED_WRR)
        return;
    if (--p->wrr.time_slice)
        return;
    static char group_path[4096];
    cgroup_path(task_group(p)->css.cgroup,group_path,4096);
    printk("in task-tick, task group is: %s. ", group_path);
    if (group_path == "/"){
        printk("set slice to 100 by wrr\n");
        p->wrr.time_slice = WRR_TIMESLICE_FG;
    }
    else{
        printk("set slice to 10 by wrr\n");
        p->wrr.time_slice = WRR_TIMESLICE_BG;
    }
}

/*
 * assign the time slice
 * if is a foreground task, then give more time slice
 * else,give less time
 */
static unsigned int get_rr_interval_wrr(struct rq *rq, struct task_struct *task)
{
    if(task->policy != SCHED_WRR)
        return 0;
    static char group_path[4096];
    cgroup_path(task_group(task)->css.cgroup,group_path,4096);
    if (group_path == "/"){
        printk("set slice to 100 by wrr\n");
        return WRR_TIMESLICE_FG;
    }
    else{
        printk("set slice to 10 by wrr\n");
        return WRR_TIMESLICE_BG;
    }
}

static void prio_changed_wrr(struct rq *rq, struct task_struct *p, int oldprio)
{
}

static void switched_to_wrr(struct rq *rq, struct task_struct *p)
{
}


const struct sched_class wrr_sched_class = {
        .next			    = &fair_sched_class,
        .enqueue_task		= enqueue_task_wrr,
        .dequeue_task		= dequeue_task_wrr,
        .yield_task		    = yield_task_wrr,
        .check_preempt_curr	= check_preempt_curr_wrr,
        .pick_next_task		= pick_next_task_wrr,
        .put_prev_task		= put_prev_task_wrr,

#ifdef CONFIG_SMP
.select_task_rq		= select_task_rq_wrr,
	.set_cpus_allowed       = set_cpus_allowed_wrr,
	.rq_online              = rq_online_wrr,
	.rq_offline             = rq_offline_wrr,
	.pre_schedule		= pre_schedule_wrr,
	.post_schedule		= post_schedule_wrr,
	.task_woken		= task_woken_wrr,
	.switched_from		= switched_from_wrr,
#endif
        .set_curr_task          = set_curr_task_wrr,
        .task_tick		= task_tick_wrr,
        .get_rr_interval	= get_rr_interval_wrr,

        .prio_changed		= prio_changed_wrr,
        .switched_to		= switched_to_wrr,
};
