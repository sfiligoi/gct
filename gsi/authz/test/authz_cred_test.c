#include <gaa.h>
#include "gssapi_test_utils.h"
#include "gssapi.h"
#include "globus_gsi_authz.h"
#include <gaa_gss_generic.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#define USAGE "Usage: %s service_name\n"

struct context_arg
{
    gss_cred_id_t                       credential;
    int                                 fd;
    struct sockaddr_un *                address;
};

static void *
server_func(
    void *                              arg,
    char *				servicename
    );

static void *
client_func(
    void *                              arg);

#ifdef notdef
static void
process_gaa(char *gaa_config_file, gss_ctx_id_t ctx);
#endif


static void
authtest_l_init_callback(void *				cb_arg,
			 globus_gsi_authz_handle_t 	handle,
			 globus_result_t		result);

static void
authtest_l_authorize_callback(void *				cb_arg,
			      globus_gsi_authz_handle_t 	handle,
			      globus_result_t			result);


int
main(int argc, char **argv)
{
    gss_cred_id_t                       credential;
    int                                 listen_fd;
    int                                 accept_fd;
    struct sockaddr_un *                address;
    struct context_arg *                arg = NULL;
    pid_t                               pid;
    char *				servicename = NULL;

    if (argc != 2)
    {
	fprintf(stderr, USAGE, argv[0]);
	exit(1);
    }
    servicename = argv[1];
    /* module activation */

    globus_module_activate(GLOBUS_GSI_GSS_ASSIST_MODULE);
    globus_module_activate(GLOBUS_GSI_GSSAPI_MODULE);
    globus_module_activate(GLOBUS_COMMON_MODULE);
    globus_module_activate(GLOBUS_GSI_AUTHZ_MODULE);
    
    /* setup listener */

    address = malloc(sizeof(struct sockaddr_un));

    memset(address,0,sizeof(struct sockaddr_un));

    address->sun_family = PF_UNIX;

    tmpnam(address->sun_path);
    
    listen_fd = socket(PF_UNIX, SOCK_STREAM, 0);

    bind(listen_fd, (struct sockaddr *) address, sizeof(struct sockaddr_un));

    listen(listen_fd,1);

    /* acquire credentials */

    credential = globus_gsi_gssapi_test_acquire_credential();

    if(credential == GSS_C_NO_CREDENTIAL)
    {
        fprintf(stderr,"Unable to aquire credential\n");
        exit(-1);
    }

    pid = fork();

    if(pid == 0)
    {
        /* child */
     	arg = malloc(sizeof(struct context_arg));
        
	arg->address = address;
        
	arg->credential = credential;

        client_func(arg);
    }
    else
    {
        accept_fd = accept(listen_fd,NULL,0);
        
	if(accept_fd < 0)
	{
	    abort();
	}
	
	arg = malloc(sizeof(struct context_arg));
        
	arg->fd = accept_fd;
        
	arg->credential = credential;

        server_func(arg, servicename);
    }

    /* close the listener */

    close(listen_fd);
    
    /* release credentials */
    
    globus_gsi_gssapi_test_release_credential(&credential); 
    
    /* free address */
    
    free(address);
    
    globus_module_deactivate(GLOBUS_COMMON_MODULE);
    globus_module_deactivate(GLOBUS_GSI_GSSAPI_MODULE);
    globus_module_deactivate(GLOBUS_GSI_GSS_ASSIST_MODULE);

    exit(0);
}


void *
server_func(
    void *                              arg,
    char *				servicename)
{
    struct context_arg *                server_args;
    globus_bool_t                       boolean_result;
    gss_ctx_id_t                        context_handle = GSS_C_NO_CONTEXT;
    char *                              user_id = NULL;
    gss_cred_id_t                       delegated_cred = GSS_C_NO_CREDENTIAL;
    globus_result_t			result;
    globus_gsi_authz_handle_t		authz_handle;
    
    server_args = (struct context_arg *) arg;

    boolean_result = globus_gsi_gssapi_test_authenticate(
	server_args->fd,
	GLOBUS_TRUE, 
	server_args->credential, 
	&context_handle, 
	&user_id, 
	&delegated_cred);
    
    if(boolean_result == GLOBUS_FALSE)
    {
	fprintf(stderr, "SERVER: Authentication failed\n");
        exit(1);
    }

    if (globus_module_activate(GLOBUS_GSI_AUTHZ_MODULE) != GLOBUS_SUCCESS)
    {
	fprintf(stderr, "SERVER: activation of authz module failed\n");
	exit(1);
    }
  
    result = globus_gsi_authz_handle_init(&authz_handle,
					  servicename,
					  context_handle,
					  authtest_l_init_callback,
					  "init callback arg");
    if (result != GLOBUS_SUCCESS)
    {
	fprintf(stderr, "SERVER: globus_gsi_authz_handle_init failed: %s\n",
		globus_object_printable_to_string(globus_error_get(result)));
	exit(1);
    }

#ifdef notdef    
    result = globus_gsi_authorize(handle, action, object,
				  authtest_l_authorize_callback,
				  "authorize callback arg");
  if (result != GLOBUS_SUCCESS)
  {
      printf("authorize failed\n");
  }


    process_authz(gaa_config_file, context_handle);

    close(server_args->fd);
    
    free(server_args);
    
    globus_gsi_gssapi_test_cleanup(&context_handle,
				   user_id,
				   &delegated_cred);
#endif    
    return NULL;
}

void *
client_func(
    void *                              arg)
{
    struct context_arg *                client_args;
    globus_bool_t                       result;
    gss_ctx_id_t                        context_handle = GSS_C_NO_CONTEXT;
    char *                              user_id = NULL;
    gss_cred_id_t                       delegated_cred = GSS_C_NO_CREDENTIAL;
    int                                 connect_fd;
    int                                 rc;

    client_args = (struct context_arg *) arg;

    connect_fd = socket(PF_UNIX, SOCK_STREAM, 0);

    rc = connect(connect_fd,
                 (struct sockaddr *) client_args->address,
                 sizeof(struct sockaddr_un));

    if(rc != 0)
    {
        abort();
    }


    result = globus_gsi_gssapi_test_authenticate(
        connect_fd,
        GLOBUS_FALSE, 
        client_args->credential, 
        &context_handle, 
        &user_id, 
        &delegated_cred);
    
    if(result == GLOBUS_FALSE)
    {
        fprintf(stderr, "CLIENT: Authentication failed\n");
        exit(1);
    }

    globus_gsi_gssapi_test_cleanup(&context_handle,
                                   user_id,
                                   &delegated_cred);
    user_id = NULL;
    
    close(connect_fd);

    free(client_args);

    return NULL;
}

#ifdef notdef
static void
process_gaa(char *gaa_config_file, gss_ctx_id_t ctx)
{
    OM_uint32				minor_status;
    gss_buffer_set_t 			data_set = 0;
    int 				i;
    gaa_ptr				gaa = 0;
    gaa_status				status;
    char *				assertion = 0;
    void *				getpolicy_param;
    char				buf[1024];
    char				rbuf[8196];
    gaa_sc_ptr 				sc = 0;
    gaa_policy *			policy = 0;
    char *				repl = 0;
    gaa_gss_generic_param_s		gaa_gss_param;

    if (gss_inquire_sec_context_by_oid(&minor_status,
				       ctx,
				       (gss_OID) saml_extension,
				       &data_set))
    {
	fprintf(stderr, "SERVER: inquire_sec_context_by_oid failed\n");
	exit(1);
    }

    if (data_set->count < 1)
    {
	printf("no saml extension, skipping gaa\n");
	return;
    }

    for (i = 0; i < data_set->count; i++)
    {
	printf("saml extension %d\n", i);
	if (assertion = data_set->elements[i].value)
	    break;
    }

    if (! assertion)
    {
	printf("saml extension found, but no saml assertion\n");
	return;
    }

    printf("Assertion:\n%s\n", assertion);
	
    if ((status = gaa_initialize(&gaa, (void *)gaa_config_file)) != GAA_S_SUCCESS) {
	fprintf(stderr, "init_gaa failed: %s: %s\n",
		gaa_x_majstat_str(status), gaa_get_err());
	exit(1);
    }

    if ((status = gaa_x_get_getpolicy_param(gaa, &getpolicy_param)) != GAA_S_SUCCESS) {
	fprintf(stderr, "getpolicy plugin not configured");
	exit(1);
    }

    if (getpolicy_param)
	*((char **)getpolicy_param) = assertion;

    gaa_gss_param.type = GAA_GSS_GENERIC_CTX;
    gaa_gss_param.param.ctx = ctx;

    if (status = init_sc(gaa, &sc, &gaa_gss_param))
    {
	fprintf(stderr, "gaa sc init failed\n");
	exit(1);
    }

    printf("> ");
    while (fgets(buf, sizeof(buf), stdin)) {
	repl = process_msg(gaa, &sc, buf, rbuf, sizeof(rbuf), &users, &policy);
	if (repl == 0)
	    printf("(null reply)");
	else
	    printf("%s", repl);
	printf("> ");
    }
    exit(0);
}
#endif

static void
authtest_l_init_callback(void *				cb_arg,
			 globus_gsi_authz_handle_t 	handle,
			 globus_result_t		result)
{
    printf("in authtest_l_init_callback, arg is %s\n", (char *)cb_arg);
}

static void
authtest_l_authorize_callback(void *				cb_arg,
			      globus_gsi_authz_handle_t 	handle,
			      globus_result_t			result)
{
    printf("in authtest_l_authorize_callback, arg is %s\n", (char *)cb_arg);
}
