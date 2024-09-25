#ifndef _MRTL_COMMON_TASK_H_
#define _MRTL_COMMON_TASK_H_

#include <mrtl/common/rbtree.h>
#include <stdint.h>

#define TASK_KEY_LEN 64

typedef int (*task_func_t)(void *, uint64_t, void *);

struct Task
{
    uint64_t nsecs;
    task_func_t func;
    void * arg;
    int8_t done;

    char key [TASK_KEY_LEN];

    struct rb_node n;
};

int generate_task_key ( char *, int, uint64_t, task_func_t, void * );

int cmp_task_key ( const struct rb_node *, const void * );

int cmp_task ( const struct rb_node *, const void * );

struct Task * add_task ( struct rb_root *, uint64_t, struct Task *, uint64_t, task_func_t, void * );

struct Task * set_next_task ( struct rb_root *, struct Task * );

int tasks_destroy ( struct rb_root * );


struct TaskManager
{
    struct rb_root task_root;
    struct Task * next_task;
    size_t task_count;
    size_t tasks_run;
};

int task_manager_init ( struct TaskManager * );

int task_manager_fini ( struct TaskManager * );

int task_manager_add_task (
    struct TaskManager * tm,
    uint64_t current_nsecs,
    uint64_t task_nsecs,
    task_func_t func,
    void * arg );

int task_manager_run_tasks_before ( struct TaskManager *, uint64_t, void * );

#endif  // _MRTL_COMMON_TASK_H_

