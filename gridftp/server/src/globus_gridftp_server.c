
#include "globus_xio.h"
#include "globus_xio_tcp_driver.h"
#include "globus_xio_ftp_cmd.h"
#include "globus_i_gridftp_server.h"

static globus_cond_t                    globus_l_gfs_cond;
static globus_mutex_t                   globus_l_gfs_mutex;
static globus_bool_t                    globus_l_gfs_terminated = GLOBUS_FALSE;
static int                              globus_l_gfs_open_count = 0;
static globus_xio_driver_t              globus_l_gfs_tcp_driver = GLOBUS_NULL;
static globus_xio_driver_t              globus_l_gfs_ftp_cmd_driver = GLOBUS_NULL;
static globus_xio_server_t              globus_l_gfs_xio_server = GLOBUS_NULL;
static globus_bool_t                    globus_l_gfs_xio_server_accepting;

static
void
globus_l_gfs_check_log_and_die(
    globus_result_t                     result);
    
static
void
globus_l_gfs_terminate_server(
    globus_bool_t                       immediately);
    
static
void
globus_l_gfs_server_closed(void);
    
/* now have an open channel (when we get here, we hand off to the
 * control or data server code)
 * XXX all thats left for process management is to setuid iff this is an inetd
 * instance (or spawned from this daemon code)
 */
static
void
globus_l_gfs_new_server_cb(
    globus_xio_handle_t                 handle,
    globus_result_t                     result,
    void *                              user_arg)
{
    char *                              remote_contact;
    
    if(result != GLOBUS_SUCCESS)
    {
        goto error_cb;
    }
    
    result = globus_xio_handle_cntl(
        handle,
        globus_l_gfs_tcp_driver,
        GLOBUS_XIO_TCP_GET_REMOTE_CONTACT,
        &remote_contact);
    if(result != GLOBUS_SUCCESS)
    {
        goto error_peername;
    }
    
    globus_i_gfs_log_message(
        GLOBUS_I_GFS_LOG_INFO, "New connection from: %s\n", remote_contact);
    
    if(globus_i_gfs_config_bool("data_node"))
    {
        /* here is where I would start a new 'data node server' */
        globus_assert(0 && "Data node channel not written");
        exit(1);
    }
    else
    {
        result = globus_i_gfs_control_start(handle, remote_contact);
    }
    
    if(result != GLOBUS_SUCCESS)
    {
        goto error_start;
    }
    
    globus_free(remote_contact);
    return;

error_start:
    globus_free(remote_contact);
    
error_peername:
error_cb:
    globus_l_gfs_server_closed();
    if(!globus_l_gfs_xio_server)
    {
        /* I am the only one expected to run, die */
        globus_l_gfs_terminate_server(GLOBUS_TRUE);
    }
}

/* begin new server */
static
globus_result_t
globus_l_gfs_open_new_server(
    globus_xio_target_t                 target)
{
    globus_result_t                     result;
    globus_xio_handle_t                 handle;
    
    if(!globus_i_gfs_config_bool("data_node"))
    {
        result = globus_xio_target_cntl(
            target,
            globus_l_gfs_ftp_cmd_driver,
            GLOBUS_XIO_DRIVER_FTP_CMD_BUFFER,
            GLOBUS_TRUE);
        if(result != GLOBUS_SUCCESS)
        {
            goto error_cntl;
        }
    }
    
    globus_mutex_lock(&globus_l_gfs_mutex);
    {
        globus_l_gfs_open_count++;
    }
    globus_mutex_unlock(&globus_l_gfs_mutex);
    
    /* dont need the handle here, will get it in callback too */
    result = globus_xio_register_open(
        &handle,
        GLOBUS_NULL,
        target,
        globus_l_gfs_new_server_cb,
        GLOBUS_NULL);
    if(result != GLOBUS_SUCCESS)
    {
        goto error_open;
    }
    
    return GLOBUS_SUCCESS;

error_open:
    globus_l_gfs_server_closed();

error_cntl:
    return result;
}

static
void
globus_l_gfs_prepare_stack(
    globus_xio_stack_t *                stack)
{
    globus_result_t                     result;
    
    result = globus_xio_stack_init(stack, GLOBUS_NULL);
    globus_l_gfs_check_log_and_die(result);
    
    result = globus_xio_stack_push_driver(*stack, globus_l_gfs_tcp_driver);
    globus_l_gfs_check_log_and_die(result);
    
    if(!globus_i_gfs_config_bool("data_node"))
    {
        result = globus_xio_stack_push_driver(
            *stack, globus_l_gfs_ftp_cmd_driver);
        globus_l_gfs_check_log_and_die(result);
    }
}

/* begin a server instance from the channel already connected on stdin */
static
void
globus_l_gfs_convert_inetd_handle(void)
{
    globus_result_t                     result;
    globus_xio_stack_t                  stack;
    globus_xio_attr_t                   attr;
    globus_xio_target_t                 target;
    
    globus_l_gfs_prepare_stack(&stack);
    
    result = globus_xio_attr_init(&attr);
    globus_l_gfs_check_log_and_die(result);
    
    result = globus_xio_attr_cntl(
        attr,
        globus_l_gfs_tcp_driver,
        GLOBUS_XIO_TCP_SET_HANDLE,
        STDIN_FILENO);
    globus_l_gfs_check_log_and_die(result);
    


    result = globus_xio_target_init(&target, attr, "", stack);
    globus_l_gfs_check_log_and_die(result);
    globus_xio_stack_destroy(stack);
    globus_xio_attr_destroy(attr);
    
    result = globus_l_gfs_open_new_server(target);
    globus_l_gfs_check_log_and_die(result);
}

/* a new client has connected */
static
void
globus_l_gfs_server_accept_cb(
    globus_xio_server_t                 server,
    globus_xio_target_t                 target,
    globus_result_t                     result,
    void *                              user_arg)
{
    if(result != GLOBUS_SUCCESS)
    {
        goto error_accept;
    }
    
    if(globus_i_gfs_config_bool("fork"))
    {
        globus_assert(0 && "forking code not here yet");
        /* be sure to destroy target on server proc and close server on 
         * client proc (close on exec)
         *
         * need to handle proc exits and decrement the open server count
         * to keep the limit in effect.
         * 
         * win32 will have to simulate fork/exec... should i just do that
         * for unix too?
         */
    }
    else
    {
        result = globus_l_gfs_open_new_server(target);
        if(result != GLOBUS_SUCCESS)
        {
            globus_i_gfs_log_result("Could not open new handle", result);
            /* we're not going to terminate the server just because we 
             * couldnt open a single client
             */
            result = GLOBUS_SUCCESS;
        }
    }
    
    globus_mutex_lock(&globus_l_gfs_mutex);
    {
        int                             max;
        
        max = globus_i_gfs_config_int("max_connections");
        if(!globus_l_gfs_terminated &&
            (max == 0 || globus_l_gfs_open_count < max))
        {
            result = globus_xio_server_register_accept(
                server,
                GLOBUS_NULL,
                globus_l_gfs_server_accept_cb,
                GLOBUS_NULL);
            if(result != GLOBUS_SUCCESS)
            {
                goto error_register_accept;
            }
        }
        else
        {
            /* we've exceeded the open connections count.  delay further
             * accepts until an instance ends.
             * XXX this currently only affects non-fork code.
             */
            globus_l_gfs_xio_server_accepting = GLOBUS_FALSE;
        }
    }
    globus_mutex_unlock(&globus_l_gfs_mutex);
    
    return;

error_register_accept:
    globus_mutex_unlock(&globus_l_gfs_mutex);
    
error_accept:
    globus_i_gfs_log_result("Unable to accept connections", result);
    globus_l_gfs_terminate_server(GLOBUS_FALSE);
}

/* start up a daemon which will spawn server instances */
static
void
globus_l_gfs_be_daemon(void)
{
    globus_result_t                     result;
    globus_xio_stack_t                  stack;
    globus_xio_attr_t                   attr;
    
    globus_l_gfs_prepare_stack(&stack);
    
    result = globus_xio_attr_init(&attr);
    globus_l_gfs_check_log_and_die(result);
    
    result = globus_xio_attr_cntl(
        attr,
        globus_l_gfs_tcp_driver,
        GLOBUS_XIO_TCP_SET_PORT,
        globus_i_gfs_config_int("port"));
    globus_l_gfs_check_log_and_die(result);
    
    result = globus_xio_attr_cntl(
        attr,
        globus_l_gfs_tcp_driver,
        GLOBUS_XIO_TCP_SET_REUSEADDR,
        GLOBUS_TRUE);
    globus_l_gfs_check_log_and_die(result);
    
    result = globus_xio_server_create(&globus_l_gfs_xio_server, attr, stack);
    globus_l_gfs_check_log_and_die(result);
    globus_xio_stack_destroy(stack);
    globus_xio_attr_destroy(attr);
    
    if(globus_i_gfs_config_int("port") == 0 ||
        globus_i_gfs_config_bool("contact_string"))
    {
        char *                          contact_string;
        
        result = globus_xio_server_cntl(
            globus_l_gfs_xio_server,
            globus_l_gfs_tcp_driver,
            GLOBUS_XIO_TCP_GET_LOCAL_CONTACT,
            &contact_string);
        globus_l_gfs_check_log_and_die(result);
        
        globus_libc_printf("Server listening at %s\n", contact_string);
        globus_free(contact_string);
    }
    
    if(globus_i_gfs_config_bool("detach"))
    {
        /* this is where i would detach the server into the background
         * not sure how this will work for win32.  if it involves starting a
         * new process, need to set server handle to not close on exec
         */
        globus_assert(0 && "detach code not here");
    }
    
    globus_l_gfs_xio_server_accepting = GLOBUS_TRUE;
    result = globus_xio_server_register_accept(
        globus_l_gfs_xio_server,
        GLOBUS_NULL,
        globus_l_gfs_server_accept_cb,
        GLOBUS_NULL);
    globus_l_gfs_check_log_and_die(result);
}
/* XXX */
int
globus_l_gfs_file_activate(void);
int
globus_l_gfs_file_deactivate(void);

int
main(
    int                                 argc,
    char **                             argv)
{
    globus_result_t                     result;
    
    globus_module_activate(GLOBUS_XIO_MODULE);
    globus_module_activate(GLOBUS_FTP_CONTROL_MODULE);
    /* globus_module_activate(GLOBUS_GRIDFTP_SERVER_MODULE); */
/* XXX */
globus_l_gfs_file_activate();
    
    globus_i_gfs_config_init(argc, argv);
    globus_i_gfs_log_open();
    globus_mutex_init(&globus_l_gfs_mutex, GLOBUS_NULL);
    globus_cond_init(&globus_l_gfs_cond, GLOBUS_NULL);
    
    result = globus_xio_driver_load("tcp", &globus_l_gfs_tcp_driver);
    globus_l_gfs_check_log_and_die(result);
    result = globus_xio_driver_load("ftp_cmd", &globus_l_gfs_ftp_cmd_driver);
    globus_l_gfs_check_log_and_die(result);
    
    if(globus_i_gfs_config_bool("inetd"))
    {
        globus_l_gfs_convert_inetd_handle();
    }
    else
    {
        globus_l_gfs_be_daemon();
    }
    
    globus_mutex_lock(&globus_l_gfs_mutex);
    {
        while(!globus_l_gfs_terminated || globus_l_gfs_open_count > 0)
        {
            globus_cond_wait(&globus_l_gfs_cond, &globus_l_gfs_mutex);
        }
    }
    globus_mutex_unlock(&globus_l_gfs_mutex);
    
    if(globus_l_gfs_xio_server)
    {
        globus_xio_server_close(globus_l_gfs_xio_server);
    }
    
    globus_xio_driver_unload(globus_l_gfs_ftp_cmd_driver);
    globus_xio_driver_unload(globus_l_gfs_tcp_driver);
    globus_i_gfs_log_close();
    
/* XXXX */
globus_l_gfs_file_deactivate();
    globus_module_deactivate_all();
    
    return 0;
}


static
void
globus_l_gfs_terminate_server(
    globus_bool_t                       immediately)
{
    /* if(immediately), abort all current connections...
     * only applicable when not forking off children..
     * should be able to terminated forked children also
     */
    globus_mutex_lock(&globus_l_gfs_mutex);
    {
        if(!globus_l_gfs_terminated)
        {
            globus_l_gfs_terminated = GLOBUS_TRUE;
            if(globus_l_gfs_open_count == 0)
            {
                globus_cond_signal(&globus_l_gfs_cond);
            }
        }
    }
    globus_mutex_unlock(&globus_l_gfs_mutex);
}

static
void
globus_l_gfs_server_closed()
{
    globus_result_t                     result;
    
    globus_mutex_lock(&globus_l_gfs_mutex);
    {
        if(--globus_l_gfs_open_count == 0 && globus_l_gfs_terminated)
        {
            globus_cond_signal(&globus_l_gfs_cond);
        }
        else if(globus_l_gfs_xio_server &&
            !globus_l_gfs_xio_server_accepting &&
            !globus_l_gfs_terminated)
        {
            /* we must have hit the limit of open servers before...
             * the death of this one opens a slot
             */
            globus_l_gfs_xio_server_accepting = GLOBUS_TRUE;
            result = globus_xio_server_register_accept(
                globus_l_gfs_xio_server,
                GLOBUS_NULL,
                globus_l_gfs_server_accept_cb,
                GLOBUS_NULL);
            if(result != GLOBUS_SUCCESS)
            {
                goto error_accept;
            }
        }
    }
    globus_mutex_unlock(&globus_l_gfs_mutex);
        
    return;
    
error_accept:
    globus_mutex_unlock(&globus_l_gfs_mutex);
    globus_i_gfs_log_result("Unable to accept connections", result);
    globus_l_gfs_terminate_server(GLOBUS_FALSE);
}

/* check. if ! success, log and die */
static
void
globus_l_gfs_check_log_and_die(
    globus_result_t                     result)
{
    if(result != GLOBUS_SUCCESS)
    {
        globus_i_gfs_log_result("Could not start server", result);
        exit(1);
    }
}
