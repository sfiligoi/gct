#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL

#include "globus_module.h"
#include "globus_callback.h"
#include "globus_i_callback.h"
#include "globus_priority_q.h"
#include "globus_handle_table.h"
#include "globus_thread_common.h"
#include "globus_libc.h"

#define GLOBUS_L_CALLBACK_INFO_BLOCK_SIZE 32
#define GLOBUS_L_CALLBACK_SPACE_BLOCK_SIZE 16

/* this is the number of ready oneshots that will be fired after time has
 * expired in globus_callback_space_poll()
 */
#define GLOBUS_L_CALLBACK_POST_STOP_ONESHOTS 10

#ifdef TARGET_ARCH_WIN32
#define pause() Sleep(1000);
#endif

static
int
globus_l_callback_activate(void);

static
int
globus_l_callback_deactivate(void);

#include "version.h"

globus_module_descriptor_t              globus_i_callback_module =
{
    "globus_callback_nonthreaded",
    globus_l_callback_activate,
    globus_l_callback_deactivate,
    GLOBUS_NULL,
    GLOBUS_NULL,
    &local_version
};

typedef enum
{
    GLOBUS_L_CALLBACK_QUEUE_NONE,
    GLOBUS_L_CALLBACK_QUEUE_TIMED,
    GLOBUS_L_CALLBACK_QUEUE_READY
} globus_l_callback_queue_state;

typedef struct globus_l_callback_info_s
{
    globus_callback_handle_t            handle;

    globus_callback_func_t              callback_func;
    void *                              callback_args;
    
    /* start_time only filled in for oneshots with delay and periodics */ 
    globus_abstime_t                    start_time;
    /* period only filled in for periodics */ 
    globus_reltime_t                    period;
    globus_bool_t                       is_periodic;
    globus_l_callback_queue_state       in_queue;

    int                                 running_count;
    
    globus_callback_func_t              unregister_callback;
    void *                              unreg_args;

    struct globus_l_callback_space_s *  my_space;
    
    /* used by ready queue macros */
    struct globus_l_callback_info_s *   next;
} globus_l_callback_info_t;

typedef struct
{
    globus_l_callback_info_t *          head;
    globus_l_callback_info_t **         tail;
} globus_l_callback_ready_queue_t;

typedef struct globus_l_callback_space_s
{
    globus_callback_space_t             handle;
    globus_priority_q_t                 timed_queue;
    globus_l_callback_ready_queue_t     ready_queue;
} globus_l_callback_space_t;

typedef struct
{
    globus_bool_t                       restarted;
    const globus_abstime_t *            time_stop;
    globus_bool_t                       signaled;
    globus_l_callback_info_t *          callback_info;
} globus_l_callback_restart_info_t;

static globus_handle_table_t            globus_l_callback_handle_table;
static globus_handle_table_t            globus_l_callback_space_table;
static globus_memory_t                  globus_l_callback_info_memory;
static globus_memory_t                  globus_l_callback_space_memory;

static globus_l_callback_space_t        globus_l_callback_global_space;
static globus_l_callback_restart_info_t * globus_l_callback_restart_info;

/**
 * globus_l_callback_requeue
 *
 * Called by globus_l_callback_blocked_cb, globus_callback_space_poll, and
 * globus_callback_adjust_period. Used to requeue a periodic callback after it
 * has blocked or completed
 */

static
void
globus_l_callback_requeue(
    globus_l_callback_info_t *          callback_info,
    const globus_abstime_t *            time_now)
{
    globus_bool_t                       ready;
    globus_l_callback_space_t *         i_space;
    const globus_abstime_t *            tmp_time;
    globus_abstime_t                    l_time_now;
    
    ready = GLOBUS_TRUE;
    i_space = callback_info->my_space;
    
    /* first check to see if anything in the timed queue is ready */
    tmp_time = (globus_abstime_t *)
        globus_priority_q_first_priority(&i_space->timed_queue);
    if(tmp_time)
    {
        if(!time_now)
        {
            GlobusTimeAbstimeGetCurrent(l_time_now);
            time_now = &l_time_now;
        }
        
        while(tmp_time && globus_abstime_cmp(tmp_time, time_now) <= 0)
        {
            globus_l_callback_info_t *          ready_info;
            
            ready_info = (globus_l_callback_info_t *)
                globus_priority_q_dequeue(&i_space->timed_queue);
            
            ready_info->in_queue = GLOBUS_L_CALLBACK_QUEUE_READY;
            
            GlobusICallbackReadyEnqueue(&i_space->ready_queue, ready_info);
                    
            tmp_time = (globus_abstime_t *)
                globus_priority_q_first_priority(&i_space->timed_queue);
        }
    }
    
    /* see if this is going in the priority q */
    if(globus_reltime_cmp(&callback_info->period, &globus_i_reltime_zero) > 0)
    {
        if(!time_now)
        {
            GlobusTimeAbstimeGetCurrent(l_time_now);
            time_now = &l_time_now;
        }
        
        GlobusTimeAbstimeInc(callback_info->start_time, callback_info->period);
    
        if(globus_abstime_cmp(time_now, &callback_info->start_time) < 0)
        {
            ready = GLOBUS_FALSE;
            callback_info->in_queue = GLOBUS_L_CALLBACK_QUEUE_TIMED;
            
            globus_priority_q_enqueue(
                &i_space->timed_queue,
                callback_info,
                &callback_info->start_time);
        }
    }
    
    if(ready)
    {
        callback_info->in_queue = GLOBUS_L_CALLBACK_QUEUE_READY;
        
        GlobusICallbackReadyEnqueue(&i_space->ready_queue, callback_info);
    }
}

/**
 * globus_l_callback_blocked_cb
 *
 * This call is registered with globus_thread_blocking_callback_push.  It is
 * called when a user calls globus_thread_blocking_will_block/globus_cond_wait
 *
 * When called, this function will requeue a periodic callback iff .
 * globus_thread_blocking_will_block/globus_cond_wait was called on that
 * callbacks 'space' or if that callback belongs to the global space
 */

static
void
globus_l_callback_blocked_cb(
    globus_thread_callback_index_t      index,
    globus_callback_space_t             space,
    void *                              user_args)
{
    globus_l_callback_restart_info_t *  restart_info;
    
    restart_info = (globus_l_callback_restart_info_t *) user_args;
    
    if(restart_info && !restart_info->restarted)
    {
        globus_l_callback_info_t *      callback_info;

        callback_info = restart_info->callback_info;

        if(callback_info->my_space->handle == GLOBUS_CALLBACK_GLOBAL_SPACE ||
            callback_info->my_space->handle == space)
        {
            if(callback_info->is_periodic)
            {
                globus_l_callback_requeue(callback_info, GLOBUS_NULL);
            }

            restart_info->restarted = GLOBUS_TRUE;
        }
    }
}

/*
 * destructor for globus_handle_table.  called whenever the reference for
 * a callback_info goes to zero.
 */
static
void
globus_l_callback_info_destructor(
    void *                              datum)
{
    globus_l_callback_info_t *          callback_info;
    
    callback_info = (globus_l_callback_info_t *) datum;
    
    /* global space is local storage, is not managed */
    if(callback_info->my_space->handle != GLOBUS_CALLBACK_GLOBAL_SPACE)
    {
        globus_handle_table_decrement_reference(
            &globus_l_callback_space_table, callback_info->my_space->handle);
    }

    globus_memory_push_node(
        &globus_l_callback_info_memory, callback_info);
}

/* 
 * destructor for globus_handle_table.  called whenever the reference for
 * a space goes to zero.
 *
 */
static
void
globus_l_callback_space_destructor(
    void *                              datum)
{
    globus_l_callback_space_t *         space;
    
    space = (globus_l_callback_space_t *) datum;
    
    globus_priority_q_destroy(&space->timed_queue);
    
    globus_memory_push_node(
        &globus_l_callback_space_memory, space);
}

static
int
globus_l_callback_activate()
{
    int                                 rc;

    rc = globus_module_activate(GLOBUS_THREAD_MODULE);
    if(rc != GLOBUS_SUCCESS)
    {
        return rc;
    }

    globus_handle_table_init(
        &globus_l_callback_handle_table,
        globus_l_callback_info_destructor);
    
    globus_handle_table_init(
        &globus_l_callback_space_table,
        globus_l_callback_space_destructor);

    /* init global 'space' */
    globus_l_callback_global_space.handle = GLOBUS_CALLBACK_GLOBAL_SPACE;
    GlobusICallbackReadyInit(&globus_l_callback_global_space.ready_queue);
    globus_priority_q_init(
        &globus_l_callback_global_space.timed_queue,
        (globus_priority_q_cmp_func_t) globus_abstime_cmp);

    globus_memory_init(
        &globus_l_callback_info_memory,
        sizeof(globus_l_callback_info_t),
        GLOBUS_L_CALLBACK_INFO_BLOCK_SIZE);

    globus_memory_init(
        &globus_l_callback_space_memory,
        sizeof(globus_l_callback_space_t),
        GLOBUS_L_CALLBACK_SPACE_BLOCK_SIZE);

    globus_l_callback_restart_info = GLOBUS_NULL;

    return GLOBUS_SUCCESS;
}

static
int
globus_l_callback_deactivate()
{
    globus_priority_q_destroy(&globus_l_callback_global_space.timed_queue);
    
    /* any handles left here will be destroyed by destructor.
     * important that globus_l_callback_handle_table be destroyed
     * BEFORE globus_l_callback_space_table since destructor for the former
     * accesses the latter
     */
    globus_handle_table_destroy(&globus_l_callback_handle_table);
    globus_handle_table_destroy(&globus_l_callback_space_table);
    
    globus_memory_destroy(&globus_l_callback_info_memory);
    globus_memory_destroy(&globus_l_callback_space_memory);
    
    return globus_module_deactivate(GLOBUS_THREAD_MODULE);
}

/**
 * globus_l_callback_register
 *
 * called by the external register functions.
 * -- populate a callback_info structure.
 */

static
globus_result_t
globus_l_callback_register(
    globus_callback_handle_t *          callback_handle,
    const globus_abstime_t *            start_time,
    const globus_reltime_t *            period,
    globus_callback_func_t              callback_func,
    void *                              callback_user_args,
    globus_callback_space_t             space)
{
    globus_l_callback_info_t *          callback_info;

    if(!callback_func)
    {
        return GLOBUS_L_CALLBACK_CONSTRUCT_INVALID_ARGUMENT(
            "globus_callback_space_register_oneshot", "callback_func");
    }

    callback_info = (globus_l_callback_info_t *)
        globus_memory_pop_node(&globus_l_callback_info_memory);
    if(!callback_info)
    {
        return GLOBUS_L_CALLBACK_CONSTRUCT_MEMORY_ALLOC(
            "globus_l_callback_register", "callback_info");
    }

    if(space == GLOBUS_CALLBACK_GLOBAL_SPACE)
    {
        callback_info->my_space = &globus_l_callback_global_space;
    }
    else
    {
        /* get internal space structure and increment its ref count */
        globus_l_callback_space_t *     i_space;

        i_space = (globus_l_callback_space_t *)
            globus_handle_table_lookup(
                &globus_l_callback_space_table, space);
        if(!i_space)
        {
            globus_memory_push_node(
                &globus_l_callback_info_memory, callback_info);

            return GLOBUS_L_CALLBACK_CONSTRUCT_INVALID_SPACE(
                "globus_l_callback_register");
        }

        globus_handle_table_increment_reference(
            &globus_l_callback_space_table, space);

        callback_info->my_space = i_space;
    }
    
    if(callback_handle)
    {
        /* if user passed callback_handle, there are two refs to this
         * info, me and user.  User had better unregister this handle
         * to free up the memory
         */
        callback_info->handle = globus_handle_table_insert(
            &globus_l_callback_handle_table, callback_info, 2);

        *callback_handle = callback_info->handle;
    }
    else
    {
        callback_info->handle = globus_handle_table_insert(
            &globus_l_callback_handle_table, callback_info, 1);
    }
    
    callback_info->callback_func = callback_func;
    callback_info->callback_args = callback_user_args;
    callback_info->running_count = 0;
    callback_info->unregister_callback = GLOBUS_NULL;
    
    if(period)
    {
        /* periodics need valid start times */
        if(!start_time)
        {
            GlobusTimeAbstimeGetCurrent(callback_info->start_time);
        }
        GlobusTimeReltimeCopy(callback_info->period, *period);
        callback_info->is_periodic = GLOBUS_TRUE;
    }
    else
    {
        callback_info->is_periodic = GLOBUS_FALSE;
    }
    
    if(start_time)
    {
        if(globus_time_abstime_is_infinity(start_time))
        {
            /* this will never run... must be a periodic that will be
             * restarted with globus_callback_adjust_period()
             */
            callback_info->in_queue = GLOBUS_L_CALLBACK_QUEUE_NONE;
            
            /* if the user didnt pass a handle in for this, then
             * this will cause the callback_info to be freed
             * -- user doesnt know what they're doing, but no harm
             * done
             */
            globus_handle_table_decrement_reference(
               &globus_l_callback_handle_table, callback_info->handle);
        }
        else
        {
            GlobusTimeAbstimeCopy(callback_info->start_time, *start_time);
            callback_info->in_queue = GLOBUS_L_CALLBACK_QUEUE_TIMED;
            
            globus_priority_q_enqueue(
                &callback_info->my_space->timed_queue,
                callback_info,
                &callback_info->start_time);
        }
    }
    else
    {
        callback_info->in_queue = GLOBUS_L_CALLBACK_QUEUE_READY;
        
        GlobusICallbackReadyEnqueue(
            &callback_info->my_space->ready_queue, callback_info);
    }
    
    return GLOBUS_SUCCESS;
}

/**
 * globus_callback_space_register_oneshot
 *
 * external function that registers a one shot some delay from now.
 *
 */

globus_result_t
globus_callback_space_register_oneshot(
    globus_callback_handle_t *          callback_handle,
    const globus_reltime_t *            delay_time,
    globus_callback_func_t              callback_func,
    void *                              callback_user_args,
    globus_callback_space_t             space)
{
    globus_abstime_t                    start_time;
    
    if(delay_time)
    {
        if(globus_reltime_cmp(delay_time, &globus_i_reltime_zero) <= 0)
        {
            delay_time = GLOBUS_NULL;
        }
        else if(globus_time_reltime_is_infinity(delay_time))
        {
            /* user is being goofy here, but I'll allow it */
            GlobusTimeAbstimeCopy(start_time, globus_i_abstime_infinity);
        }
        else
        {
            GlobusTimeAbstimeGetCurrent(start_time);
            GlobusTimeAbstimeInc(start_time, *delay_time);
        }
    }

    return globus_l_callback_register(
        callback_handle,
        delay_time
            ? &start_time
            : GLOBUS_NULL,
        GLOBUS_NULL,
        callback_func,
        callback_user_args,
        space);
}

/**
 * globus_callback_space_register_periodic
 *
 * external function that registers a periodic to start some delay from now.
 *
 */

globus_result_t
globus_callback_space_register_periodic(
    globus_callback_handle_t *          callback_handle,
    const globus_reltime_t *            delay_time,
    const globus_reltime_t *            period,
    globus_callback_func_t              callback_func,
    void *                              callback_user_args,
    globus_callback_space_t             space)
{
    globus_abstime_t                    start_time;

    if(!period)
    {
        return GLOBUS_L_CALLBACK_CONSTRUCT_INVALID_ARGUMENT(
            "globus_callback_space_register_periodic", "period");
    }
    
    if(delay_time)
    {
        if(globus_reltime_cmp(delay_time, &globus_i_reltime_zero) <= 0)
        {
            delay_time = GLOBUS_NULL;
        }
        else if(globus_time_reltime_is_infinity(delay_time))
        {
            GlobusTimeAbstimeCopy(start_time, globus_i_abstime_infinity);
        }
        else
        {
            GlobusTimeAbstimeGetCurrent(start_time);
            GlobusTimeAbstimeInc(start_time, *delay_time);
        }
    }
    
    if(globus_time_reltime_is_infinity(period))
    {
        /* infinite periods start life out as a oneshot,
         * globus_callback_adjust_period() is used to revive them
         */
        period = GLOBUS_NULL;
    }
    
    return globus_l_callback_register(
        callback_handle,
        delay_time
            ? &start_time
            : GLOBUS_NULL,
        period,
        callback_func,
        callback_user_args,
        space);
}

/**
 * globus_l_callback_cancel_kickout
 *
 * driver callback to kickout unregister callback. registered by
 * globus_callback_register_cancel
 *
 * This is only going to get registered if the canceled callback was not
 * running or already complete.
 */

static
void
globus_l_callback_cancel_kickout_cb(
    void *                              user_args)
{
    globus_l_callback_info_t *          callback_info;

    callback_info = (globus_l_callback_info_t *) user_args;

    callback_info->unregister_callback(callback_info->unreg_args);

    /* this will cause the callback_info to be freed */
    globus_handle_table_decrement_reference(
        &globus_l_callback_handle_table, callback_info->handle);
}

/**
 * globus_callback_unregister
 *
 * external function that cancels a previously registered callback.  will not
 * interrupt an already running callback.  also handles case where callback has
 * already completed.  it is safe to call this within the callback
 * that is being cancelled.
 *
 * the combination of this func and adjust period may cause some confusion in
 * understanding the operation.  remember that adjust period can make a
 * callback appear to be a oneshot (if adjust period is passed a null period,
 * is_periodic will become false)... 
 */

globus_result_t
globus_callback_unregister(
    globus_callback_handle_t            callback_handle,
    globus_callback_func_t              unregister_callback,
    void *                              unreg_args,
    globus_bool_t *                     active)
{
    globus_l_callback_info_t *          callback_info;

    callback_info = (globus_l_callback_info_t *)
        globus_handle_table_lookup(
                &globus_l_callback_handle_table, callback_handle);

    if(!callback_info)
    {
        /* this is definitely an error,
         * if user had the handle and didnt destroy it (or cancel it),
         * it has to exist
         */
        return GLOBUS_L_CALLBACK_CONSTRUCT_INVALID_CALLBACK_HANDLE(
            "globus_callback_unregister");
    }

    /* this doesnt catch a previously unregistered callback that passed a
     * NULL unregister -- bad things may happen in that case
     */
    if(callback_info->unregister_callback)
    {
        return GLOBUS_L_CALLBACK_CONSTRUCT_ALREADY_CANCELED(
            "globus_callback_unregister");
    }

    callback_info->unregister_callback = unregister_callback;
    callback_info->unreg_args = unreg_args;

    if(callback_info->running_count > 0)
    {
        if(callback_info->is_periodic)
        {
            /* would only be in queue if it was restarted */
            if(callback_info->in_queue == GLOBUS_L_CALLBACK_QUEUE_TIMED)
            {
                globus_priority_q_remove(
                    &callback_info->my_space->timed_queue, callback_info);
            }
            else if(callback_info->in_queue == GLOBUS_L_CALLBACK_QUEUE_READY)
            {
                GlobusICallbackReadyRemove(
                    &callback_info->my_space->ready_queue, callback_info);
            }
            
            callback_info->in_queue = GLOBUS_L_CALLBACK_QUEUE_NONE;
            callback_info->is_periodic = GLOBUS_FALSE;
        }

        /* unregister callback will get registered when running_count == 0 */

        /* this decrements the user's reference */
        globus_handle_table_decrement_reference(
            &globus_l_callback_handle_table, callback_handle);
        
        if(active)
        {
            *active = GLOBUS_TRUE;
        }
        
        return GLOBUS_SUCCESS;
    }
    else
    {
        /*
         * if the callback_info is not in the queue, it can only mean
         * that it has been suspended (by adjust_period) or it was a oneshot.
         * In this case, I would have already decremented the ref once.  I'll
         * let the globus_l_callback_cancel_kickout_cb decr the last ref
         */
        if(callback_info->in_queue)
        {
            if(callback_info->in_queue == GLOBUS_L_CALLBACK_QUEUE_TIMED)
            {
                globus_priority_q_remove(
                    &callback_info->my_space->timed_queue, callback_info);
            }
            else if(callback_info->in_queue == GLOBUS_L_CALLBACK_QUEUE_READY)
            {
                GlobusICallbackReadyRemove(
                    &callback_info->my_space->ready_queue, callback_info);
            }
            
            globus_handle_table_decrement_reference(
                &globus_l_callback_handle_table, callback_handle);
                    
            callback_info->in_queue = GLOBUS_L_CALLBACK_QUEUE_NONE;
        }
        
        if(unregister_callback)
        {
            globus_callback_space_register_oneshot(
                GLOBUS_NULL,
                GLOBUS_NULL,
                globus_l_callback_cancel_kickout_cb,
                callback_info,
                callback_info->my_space->handle);
        }
        else
        {
            /* not kicking one out, so decr last ref */
            globus_handle_table_decrement_reference(
                &globus_l_callback_handle_table, callback_handle);
        }
        
        if(active)
        {
            *active = GLOBUS_FALSE;
        }
        
        return GLOBUS_SUCCESS;
    }
}

/**
 * globus_callback_adjust_period
 *
 * external function to allow a user to adjust the period of a previously
 * registered callback.  it is safe to call this within or outside of
 * the callback that is being modified.
 *
 * this func also allows a user to 'suspend' a periodic callback till another
 * time by passing a period of globus_i_reltime_infinity.  the callback can
 * be resumed by passing in a new period at some other time.
 *
 * this function could cause confusion in understanding this code.  When a
 * periodic is suspended, it 'becomes' non-periodic (ie, is_periodic is set to
 * false)
 */

globus_result_t
globus_callback_adjust_period(
    globus_callback_handle_t            callback_handle,
    const globus_reltime_t *            new_period)
{
    globus_l_callback_info_t *          callback_info;

    callback_info = (globus_l_callback_info_t *)
        globus_handle_table_lookup(
            &globus_l_callback_handle_table, callback_handle);
    if(!callback_info)
    {
        return GLOBUS_L_CALLBACK_CONSTRUCT_INVALID_CALLBACK_HANDLE(
            "globus_callback_adjust_period");
    }
    
    /* this doesnt catch a previously unregistered callback that passed a
     * NULL unregister -- bad things may happen in that case
     */
    if(callback_info->unregister_callback)
    {
        return GLOBUS_L_CALLBACK_CONSTRUCT_ALREADY_CANCELED(
            "globus_callback_unregister");
    }
    
    if(!new_period || globus_time_reltime_is_infinity(new_period))
    {
        /* doing this will cause this not to be requeued if currently running
         */
        callback_info->is_periodic = GLOBUS_FALSE;

        /* may or may not be in queue depending on if its not running or its
         * been restarted.  if its not in queue, no problem... it wont get
         * queued again
         */
        if(callback_info->in_queue)
        {
            if(callback_info->in_queue == GLOBUS_L_CALLBACK_QUEUE_TIMED)
            {
                globus_priority_q_remove(
                    &callback_info->my_space->timed_queue, callback_info);
            }
            else if(callback_info->in_queue == GLOBUS_L_CALLBACK_QUEUE_READY)
            {
                GlobusICallbackReadyRemove(
                    &callback_info->my_space->ready_queue, callback_info);
            }
            
            if(callback_info->running_count == 0)
            {
                globus_handle_table_decrement_reference(
                    &globus_l_callback_handle_table, callback_handle);
            }
                    
            callback_info->in_queue = GLOBUS_L_CALLBACK_QUEUE_NONE;
        }
    }
    else
    {
        callback_info->is_periodic = GLOBUS_TRUE;
        GlobusTimeReltimeCopy(callback_info->period, *new_period);
        
        if(globus_reltime_cmp(new_period, &globus_i_reltime_zero) > 0)
        {
            GlobusTimeAbstimeGetCurrent(callback_info->start_time);
            GlobusTimeAbstimeInc(callback_info->start_time, *new_period);
            
           /* may or may not be in queue depending on if its not running or its
            * been restarted.  if its not in queue and its running, no problem...
            * when it gets requeued it will be with the new priority
            */
            if(callback_info->in_queue == GLOBUS_L_CALLBACK_QUEUE_TIMED)
            {
                globus_priority_q_modify(
                    &callback_info->my_space->timed_queue,
                    callback_info,
                    &callback_info->start_time);
            }
            else if(callback_info->in_queue == GLOBUS_L_CALLBACK_QUEUE_READY)
            {
                GlobusICallbackReadyRemove(
                    &callback_info->my_space->ready_queue, callback_info);
                
                callback_info->in_queue = GLOBUS_L_CALLBACK_QUEUE_TIMED;
                
                globus_priority_q_enqueue(
                    &callback_info->my_space->timed_queue,
                    callback_info,
                    &callback_info->start_time);
            }
            else if(callback_info->running_count == 0)
            {
                /* it wasnt in the queue and its not running...  we must have
                 * previously set this non-periodic... I need to requeue it
                 * and take my ref to it back
                 */
                callback_info->in_queue = GLOBUS_L_CALLBACK_QUEUE_TIMED;
                
                globus_priority_q_enqueue(
                    &callback_info->my_space->timed_queue,
                    callback_info,
                    &callback_info->start_time);
            
                globus_handle_table_increment_reference(
                    &globus_l_callback_handle_table, callback_handle);
            }
        }
        else if(callback_info->in_queue != GLOBUS_L_CALLBACK_QUEUE_READY)
        {
            /* may or may not be in queue depending on if its not running or its
             * been restarted.  if its not in queue and its running, no problem...
             * when it gets requeued it will be with the new priority
             */
            if(callback_info->in_queue == GLOBUS_L_CALLBACK_QUEUE_TIMED)
            {
                globus_priority_q_remove(
                    &callback_info->my_space->timed_queue, callback_info);
                
                callback_info->in_queue = GLOBUS_L_CALLBACK_QUEUE_READY;
                
                GlobusICallbackReadyEnqueue(
                    &callback_info->my_space->ready_queue, callback_info);
            }
            else if(callback_info->running_count == 0)
            {
                /* it wasnt in the queue and its not running...  we must have
                 * previously set this non-periodic... I need to requeue it
                 * and take my ref to it back
                 */
                callback_info->in_queue = GLOBUS_L_CALLBACK_QUEUE_READY;
                
                GlobusICallbackReadyEnqueue(
                    &callback_info->my_space->ready_queue, callback_info);
                    
                globus_handle_table_increment_reference(
                    &globus_l_callback_handle_table, callback_handle);
            }
        }
    }

    return GLOBUS_SUCCESS;
}

/**
 * globus_callback_space_init
 *
 * -- attrs are ignored here since there are none that make sense in
 *    a non-threaded build
 *
 */

globus_result_t
globus_callback_space_init(
    globus_callback_space_t *           space,
    globus_callback_space_attr_t        attr)
{
    globus_l_callback_space_t *         i_space;

    if(!space)
    {
        return GLOBUS_L_CALLBACK_CONSTRUCT_INVALID_ARGUMENT(
            "globus_callback_space_init", "space");
    }

    i_space = (globus_l_callback_space_t *)
        globus_memory_pop_node(&globus_l_callback_space_memory);
    if(!i_space)
    {
        return GLOBUS_L_CALLBACK_CONSTRUCT_MEMORY_ALLOC(
            "globus_callback_space_init", "i_space");
    }

    GlobusICallbackReadyInit(&i_space->ready_queue);
    globus_priority_q_init(
        &i_space->timed_queue,
        (globus_priority_q_cmp_func_t) globus_abstime_cmp);

    i_space->handle =
        globus_handle_table_insert(
            &globus_l_callback_space_table, i_space, 1);

    *space = i_space->handle;

    return GLOBUS_SUCCESS;
}

globus_result_t
globus_callback_space_reference(
    globus_callback_space_t             space)
{
    globus_bool_t                       in_table;
    
    if(space == GLOBUS_CALLBACK_GLOBAL_SPACE)
    {
        return GLOBUS_SUCCESS;
    }
    
    in_table = globus_handle_table_increment_reference(
        &globus_l_callback_space_table, space);
        
    if(!in_table)
    {
        return GLOBUS_L_CALLBACK_CONSTRUCT_INVALID_SPACE(
            "globus_callback_space_reference");
    }

    return GLOBUS_SUCCESS;
}

/**
 * globus_callback_space_destroy
 *
 * while it does not make sense to do so, this can be called while there are
 * still pending callbacks within this space.  The space will not really be
 * destroyed untill all callbacks referencing it are destroyed.
 */

globus_result_t
globus_callback_space_destroy(
    globus_callback_space_t             space)
{
    globus_l_callback_space_t *         i_space;
    
    if(space == GLOBUS_CALLBACK_GLOBAL_SPACE)
    {
        return GLOBUS_SUCCESS;
    }
    
    i_space = (globus_l_callback_space_t *)
        globus_handle_table_lookup(
                &globus_l_callback_space_table, space);
    if(!i_space)
    {
        return GLOBUS_L_CALLBACK_CONSTRUCT_INVALID_SPACE(
            "globus_callback_space_destroy");
    }
    
    globus_handle_table_decrement_reference(
        &globus_l_callback_space_table, space);

    return GLOBUS_SUCCESS;
}

/**
 * globus_callback_space_attr_*
 *
 * -- All of these are no-ops since there arent any meaning full attrs
 *      in a non-threaded build
 */

globus_result_t
globus_callback_space_attr_init(
    globus_callback_space_attr_t *      attr)
{
    return GLOBUS_SUCCESS;
}

globus_result_t
globus_callback_space_attr_destroy(
    globus_callback_space_attr_t        attr)
{
    return GLOBUS_SUCCESS;
}

globus_result_t
globus_callback_space_attr_set_behavior(
    globus_callback_space_attr_t        attr,
    globus_callback_space_behavior_t    behavior)
{
    return GLOBUS_SUCCESS;
}

globus_result_t
globus_callback_space_attr_get_behavior(
    globus_callback_space_attr_t        attr,
    globus_callback_space_behavior_t *  behavior)
{
    if(!behavior)
    {
        return GLOBUS_L_CALLBACK_CONSTRUCT_INVALID_ARGUMENT(
            "globus_callback_space_attr_get_behavior", "behavior");
    }

    *behavior = GLOBUS_CALLBACK_SPACE_BEHAVIOR_SINGLE;

    return GLOBUS_SUCCESS;
}

/**
 * globus_l_callback_get_next
 *
 * check queue for ready entry, pass back next ready time if no callback ready
 * else return callback info.
 *
 */

static
globus_l_callback_info_t *
globus_l_callback_get_next(
    globus_l_callback_space_t *         i_space,
    const globus_abstime_t *            time_now,
    globus_abstime_t *                  ready_time)
{
    const globus_abstime_t *            tmp_time;
    globus_l_callback_info_t *          callback_info;

    /* first check to see if anything in the timed queue is ready */
    tmp_time = (globus_abstime_t *)
        globus_priority_q_first_priority(&i_space->timed_queue);
    if(tmp_time)
    {
        while(tmp_time && globus_abstime_cmp(tmp_time, time_now) <= 0)
        {
            callback_info = (globus_l_callback_info_t *)
                globus_priority_q_dequeue(&i_space->timed_queue);
            
            callback_info->in_queue = GLOBUS_L_CALLBACK_QUEUE_READY;
            
            GlobusICallbackReadyEnqueue(&i_space->ready_queue, callback_info);
                    
            tmp_time = (globus_abstime_t *)
                globus_priority_q_first_priority(&i_space->timed_queue);
        }
    }

    GlobusICallbackReadyDequeue(&i_space->ready_queue, callback_info);

    if(callback_info)
    {
        callback_info->in_queue = GLOBUS_L_CALLBACK_QUEUE_NONE;
    }
    else if(tmp_time)
    {
        GlobusTimeAbstimeCopy(*ready_time, *tmp_time);
    }
    else
    {
        GlobusTimeAbstimeCopy(*ready_time, globus_i_abstime_infinity);
    }
            
    return callback_info;
}

/**
 * globus_callback_space_poll
 *
 * external function to poll for callbacks.  will poll at least the passed
 * space.  may also poll global 'space'
 *
 */

void
globus_callback_space_poll(
    const globus_abstime_t *            timestop,
    globus_callback_space_t             space)
{
    globus_bool_t                       done;
    globus_l_callback_space_t *         i_space;
    globus_abstime_t                    time_now;
    globus_l_callback_restart_info_t *  last_restart_info;
    globus_l_callback_restart_info_t    restart_info;
    globus_thread_callback_index_t      idx;
    int                                 post_stop_counter;

    i_space = GLOBUS_NULL;

    if(space != GLOBUS_CALLBACK_GLOBAL_SPACE)
    {
        i_space = (globus_l_callback_space_t *)
            globus_handle_table_lookup(
                &globus_l_callback_space_table, space);
    }
    
    last_restart_info = globus_l_callback_restart_info;
    globus_l_callback_restart_info = &restart_info;
    
    globus_thread_blocking_callback_push(
        globus_l_callback_blocked_cb,
        &restart_info,
        &idx);
    
    if(!timestop)
    {
        timestop = &globus_i_abstime_zero;
    }
    
    /*
     * If we get signaled, we will jump out of this function asap
     */
    restart_info.signaled = GLOBUS_FALSE;
    restart_info.time_stop = timestop;
    
    GlobusTimeAbstimeGetCurrent(time_now);
    
    done = GLOBUS_FALSE;
    post_stop_counter = GLOBUS_L_CALLBACK_POST_STOP_ONESHOTS;
    
    do
    {
        globus_l_callback_info_t *      callback_info;
        globus_abstime_t                space_ready_time;
        globus_abstime_t                global_ready_time;

        callback_info = GLOBUS_NULL;

        /* first we'll see if there is a callback ready on the polled space */
        if(i_space)
        {
            callback_info = globus_l_callback_get_next(
                i_space, &time_now, &space_ready_time);
        }

        /* if we didnt get one from the polled space, check the global queue */
        if(!callback_info)
        {
            callback_info = globus_l_callback_get_next(
                &globus_l_callback_global_space,
                &time_now,
                &global_ready_time);
        }

        if(callback_info)
        {
            /* we got a callback, kick it out */
            restart_info.restarted = GLOBUS_FALSE;
            restart_info.callback_info = callback_info;

            callback_info->running_count++;

            callback_info->callback_func(callback_info->callback_args);

            callback_info->running_count--;
            
            GlobusTimeAbstimeGetCurrent(time_now);
            
            /* a periodic that was canceled has is_periodic == false */
            if(!callback_info->is_periodic &&
                callback_info->running_count == 0)
            {
                /* if this was unregistered, we need to register for the
                 * unregister callback.  the kickout will decr the last ref
                 */
                if(callback_info->unregister_callback)
                {
                    globus_callback_space_register_oneshot(
                        GLOBUS_NULL,
                        GLOBUS_NULL,
                        globus_l_callback_cancel_kickout_cb,
                        callback_info,
                        callback_info->my_space->handle);
                }
                else
                {
                    /* no unreg callback so I'll decrement my ref */
                    globus_handle_table_decrement_reference(
                       &globus_l_callback_handle_table, callback_info->handle);
                }
            }
            else if(callback_info->is_periodic && !restart_info.restarted)
            {
                globus_l_callback_requeue(callback_info, &time_now);
            }
            
            done = restart_info.signaled;
            if(!done && globus_abstime_cmp(timestop, &time_now) <= 0)
            {
                globus_l_callback_info_t *  peek;
               
                /* time has expired, but we'll call up to 
                 * GLOBUS_L_CALLBACK_POST_STOP_ONESHOTS oneshots
                 * that are ready to go
                 */
                peek = GLOBUS_NULL;
                if(i_space)
                {
                    GlobusICallbackReadyPeak(&i_space->ready_queue, peek);
                }
                if(!peek)
                {
                    GlobusICallbackReadyPeak(
                        &globus_l_callback_global_space.ready_queue, peek);
                }
                
                if(!peek || peek->is_periodic || post_stop_counter-- == 0)
                {
                    done = GLOBUS_TRUE;
                }
            }
        }
        else
        {
            globus_abstime_t *          first_ready_time;
            
            /* pick whoever's next is ready first */
            first_ready_time = &global_ready_time;
            if(i_space)
            {
                if(globus_abstime_cmp(
                    &global_ready_time, &space_ready_time) > 0)
                {
                    first_ready_time = &space_ready_time;
                }
            }
        
            /* no callbacks were ready */
            if(globus_abstime_cmp(timestop, first_ready_time) > 0)
            {
                /* sleep until first one is ready */
                globus_reltime_t        sleep_time;
                unsigned long           usec;

                GlobusTimeAbstimeDiff(sleep_time, *first_ready_time, time_now);
                GlobusTimeReltimeToUSec(usec, sleep_time);

                if(usec > 0)
                {
                    globus_libc_usleep(usec);
                }
            }
            else if(globus_time_abstime_is_infinity(timestop))
            {
                /* we can only get here if both queues are empty
                 * and we are blocking forever. in this case, it is not
                 * possible for a new callback to be registered, except by
                 * a signal handler. pause will wake up in that case
                 */
                 pause();
            }
            else
            {
                /* wont be any ready before our time is up */
                done = GLOBUS_TRUE;
            }
            
            if(!done)
            {
                GlobusTimeAbstimeGetCurrent(time_now);
                if(globus_abstime_cmp(timestop, &time_now) <= 0)
                {
                    done = GLOBUS_TRUE;
                }
            }
        }
    } while(!done);

    /*
     * If I was signaled, I need to pass that signal on to my parent poller
     * because I cant be sure that the signal was just for me
     */
    if(last_restart_info && restart_info.signaled)
    {
        last_restart_info->signaled = GLOBUS_TRUE;
    }
    
    globus_l_callback_restart_info = last_restart_info;
    
    globus_thread_blocking_callback_pop(&idx);
}

void
globus_callback_signal_poll()
{
    if(globus_l_callback_restart_info)
    {
        globus_l_callback_restart_info->signaled = GLOBUS_TRUE;
    }
}

/**
 * globus_callback_space_get
 *
 * allow a user to get the current space from within a callback
 */
globus_result_t
globus_callback_space_get(
    globus_callback_space_t *           space)
{
    if(!space)
    {
        return GLOBUS_L_CALLBACK_CONSTRUCT_INVALID_ARGUMENT(
            "globus_callback_space_get", "space");
    }
    
    if(!globus_l_callback_restart_info)
    {
        return GLOBUS_L_CALLBACK_CONSTRUCT_NO_ACTIVE_CALLBACK(
            "globus_callback_space_get");
    }
    
    *space = globus_l_callback_restart_info->callback_info->my_space->handle;
    
    return GLOBUS_SUCCESS;
}

globus_bool_t
globus_callback_space_is_single(
    globus_callback_space_t             space)
{
    return GLOBUS_TRUE;
}

/**
 * globus_callback_get_timeout
 *
 * returns true if already timed out.. remaining time is in time_left
 */

globus_bool_t
globus_callback_get_timeout(
    globus_reltime_t *                  time_left)
{
    globus_l_callback_space_t *         i_space;
    globus_l_callback_info_t *          peek;
    
    if(!globus_l_callback_restart_info)
    {
        GlobusTimeReltimeCopy(*time_left, globus_i_reltime_infinity);

        return GLOBUS_FALSE;
    }
    
    i_space = globus_l_callback_restart_info->callback_info->my_space;
    GlobusICallbackReadyPeak(&i_space->ready_queue, peek);
    if(!peek && i_space->handle != GLOBUS_CALLBACK_GLOBAL_SPACE)
    {
        GlobusICallbackReadyPeak(
            &globus_l_callback_global_space.ready_queue, peek);
    }
    
    if(peek)
    {
        GlobusTimeReltimeCopy(*time_left, globus_i_reltime_zero);
        
        return GLOBUS_TRUE;
    }
    else
    {
        globus_abstime_t                time_now;
        const globus_abstime_t *        space_time;
        const globus_abstime_t *        global_time;
        const globus_abstime_t *        earlier_time;
        
        global_time = GLOBUS_NULL;
        
        space_time = (globus_abstime_t *)
            globus_priority_q_first_priority(&i_space->timed_queue);
        if(i_space->handle != GLOBUS_CALLBACK_GLOBAL_SPACE)
        {
            global_time = (globus_abstime_t *)
                globus_priority_q_first_priority(
                    &globus_l_callback_global_space.timed_queue);
        }
        
        earlier_time = space_time;
        if(space_time && global_time)
        {
            if(globus_abstime_cmp(space_time, global_time) > 0)
            {
                earlier_time = global_time;
            }
        }
        else if(global_time)
        {
            earlier_time = global_time;
        }
        
        if(!earlier_time || globus_abstime_cmp(
            earlier_time, globus_l_callback_restart_info->time_stop) > 0)
        {
            earlier_time = globus_l_callback_restart_info->time_stop;
        }
        
        GlobusTimeAbstimeGetCurrent(time_now);
        if(globus_abstime_cmp(&time_now, earlier_time) >= 0)
        {
            GlobusTimeReltimeCopy(*time_left, globus_i_reltime_zero);
    
            return GLOBUS_TRUE;
        }
        else if(globus_time_abstime_is_infinity(earlier_time))
        {
            GlobusTimeReltimeCopy(*time_left, globus_i_reltime_infinity);
        }
        else
        {
            GlobusTimeAbstimeDiff(*time_left, time_now, *earlier_time);
        }
    }
    
    return GLOBUS_FALSE;
}

globus_bool_t
globus_callback_has_time_expired()
{
    globus_reltime_t                    time_left;
    
    return globus_callback_get_timeout(&time_left);
}

globus_bool_t
globus_callback_was_restarted()
{
    return globus_l_callback_restart_info
        ? globus_l_callback_restart_info->restarted
        : GLOBUS_FALSE;
}

#endif /* GLOBUS_DONT_DOCUMENT_INTERNAL */
