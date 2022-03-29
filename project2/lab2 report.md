# lab2 report

邬心远

519021910604

## wrr.c

In wrr.c, I mainly write the basic functions for a scheduler, include `enqueue_task_wrr`， `dequeue_task_wrr`, ` yield_task_wrr`,  `check_preempt_curr_wrr`, `pick_next_task_wrr`, ` put_prev_task_wrr`,`task_tick_wrr` and `get_rr_interval_wrr`

**enqueue_task_wrr:**

put an "wrr" type entity into the running queue and increase the `nr_running`

**dequeue_task_wrr:**

push an wrr type entity out of the running queue and decrease the `nr_running`

**yield_task_wrr:**

yield the currently running task, move it from the head of the queue to end of the queue

**check_preempt_curr_wrr:**

do nothing, because there isn't a preempt circustance for wrr

**pick_next_task:**

simply pick the next task from the head of queue and give it certain timeslice depending on whether it is forehead

**put_prev_task_wrr:**

put the task already out to the tail

**task_tick_wrr:**

called by clock terminal,  decide whether change a task and assign timesclice

**get_rr_interval_wrr:**

assign timeslice depending on whether it is forehead



Also, in this file I defined some basic functions including init functions

## sched.h(/kernel/sched)

-  wrr_rq declared and defined

  ```
  wrr_rq{
  	head_list *queue;
  	bool queue_empty;
  	unsigned long wrr_nr_running;
  	...
  	...
  }
  ```

  queue remain the ready queue, maintains all the tasks might be picked up.

- declare of wrr_sched_class and wrr_bandwidth

- put wrr_rq into rq

- declare some external functions realized in wrr.c

- define function `wrr_polic` and `task_has_wrr_policy`

  `task_has_wrr_policy` points out whether a task has wrr policy

## core.c

- implement init function into sched init function
- define `sysctl_sched_wrr_period` and `sysctl_sched_wrr_runtime`, which is same as rt scheduler, leaving opportunity to FCS
- implent prio of wrr to the decision of prio of scheduler

## sched.h(/include/linux)

- definition of `sched_wrr_entity`

  corresponding to a task and can be put in the queue of wrr scheduler

- define `WRR_TIMESLICE_FG` and `WRR_TIMESLICE_BG`, the timeslice for differnt case

- implent wrr_entity into struct task_struct

- definition of `wrr_prio` deciding whether a task is wrr case.

