#include <mrtl/common/task.h>
#include <mlog.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>


int generate_task_key (
        char * dst,
        int n,
        uint64_t nsecs,
        task_func_t func,
        void * arg )
{
    int len = snprintf( dst, n, "%lu-%lu-%lu", nsecs, (size_t)func, (size_t)arg );

    if ( len < 0 )
    {
        log_error( "generate_task_key() encountered an error writing (code %d).", len );
        return -1;
    }
    else if ( n <= len )
    {
        log_error( "generate_task_key() needed %d characters, but dst had length %d.", len, n );
        return -1;
    }

    return len;
}


int cmp_task_key ( const struct rb_node * n, const void * key )
{
    struct Task * t = container_of( n, struct Task, n);
    return strncmp( t->key, key, TASK_KEY_LEN );
}


int cmp_task ( const struct rb_node * n1, const void * n2 )
{
    const struct Task * t1 = container_of( n1, struct Task, n );
    const struct Task * t2 = container_of( n2, struct Task, n );
    return strncmp( t1->key, t2->key, TASK_KEY_LEN );
}


struct Task * add_task (
        struct rb_root * root,
        uint64_t current_nsecs,
        struct Task * next_task,
        uint64_t task_nsecs,
        task_func_t func,
        void * arg )
{
    // Add a new task to the tree, and return the next task to be run.

    if ( task_nsecs <= current_nsecs )
    {
        log_notice( "add_task() can only add future tasks." );
        return next_task;
    }

    struct rb_node * n;
    struct Task * t;

    char key [TASK_KEY_LEN];
    generate_task_key( key, TASK_KEY_LEN, task_nsecs, func, arg );

    n = rb_search( root, key, cmp_task_key );

    if ( n == NULL )
    {
        if ( (t = calloc(1, sizeof(struct Task))) == NULL )  { return NULL; }

        t->nsecs = task_nsecs;
        t->func  = func;
        t->arg   = arg;
        strncpy( t->key, key, TASK_KEY_LEN );
        rb_init_node( &t->n );

        n = rb_insert( root, &t->n, cmp_task );
    }

    t = container_of( n, struct Task, n );

    if ( NULL == next_task )
    {
        return t;
    }
    else if ( cmp_task( &t->n, &next_task->n ) < 0 )
    {
        // The new task, t, is before (less than) the next task.
        return t;
    }
    else
    {
        return next_task;
    }
}


struct Task * set_next_task ( struct rb_root * root, struct Task * t )
{
    // Return NULL if we are past the last node.

    struct Task * next_task;
    struct rb_node * n;

    if ( t == NULL )
    {
        n = rb_first( root );
    }
    else if ( &t->n == rb_last( root ) )
    {
        n = NULL;
    }
    else
    {
        n = rb_next( &t->n );
    }

    if ( n == NULL )
    {
        next_task = NULL;
    }
    else
    {
        next_task = container_of( n, struct Task, n );
    }

    return next_task;
}


int tasks_destroy ( struct rb_root * root )
{
    struct rb_node * c;
    struct Task * t;

    c = rb_first( root );

    while ( c )
    {
        t = container_of( c, struct Task, n );
        c = rb_next( &t->n );
        rb_erase( &t->n, root );
        free( t );
    }

    return 0;
}


int task_manager_init ( struct TaskManager * tm )
{
    tm->task_root.rb_node = NULL;
    tm->task_count = 0;
    tm->tasks_run = 0;
    tm->next_task = NULL;

    return 0;
}


int task_manager_fini ( struct TaskManager * tm )
{
    tasks_destroy( &tm->task_root );
    tm->task_count = 0;
    tm->next_task = NULL;

    return 0;
}


int task_manager_add_task (
        struct TaskManager * tm,
        uint64_t current_nsecs,
        uint64_t task_nsecs,
        task_func_t func,
        void * arg )
{
    tm->next_task = add_task( &tm->task_root, current_nsecs, tm->next_task, task_nsecs, func, arg );
    return 0;
}


int task_manager_run_tasks_before ( struct TaskManager * tm, uint64_t nsecs, void * proct )
{
    while ( tm->next_task && tm->next_task->nsecs < nsecs )
    {
        struct Task * t = tm->next_task;
        t->func( proct, t->nsecs, t->arg );
        t->done = 1;
        tm->tasks_run++;
        tm->next_task = set_next_task( &tm->task_root, t );
    }

    return 0;
}

