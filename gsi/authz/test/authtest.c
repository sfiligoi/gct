#include <string.h>
#include <strings.h>

#include "globus_gsi_authz.h"
#define USAGE "Usage: %s objectype object servicename action\n"

static void
authtest_l_init_callback(void *					cb_arg,
			 globus_gsi_authz_handle_t 		handle,
			 globus_result_t			result);

static void
authtest_l_authorize_callback(void *				cb_arg,
			      globus_gsi_authz_handle_t 	handle,
			      globus_result_t			result);



int
main(int argc, char **argv)
{
  globus_gsi_authz_handle_t        handle;
  char *			   cfname = 0;
  char *			   objecttype = 0;
  char *			   object = 0;
  char *			   servicename = 0;
  char *			   action = 0;
  gss_ctx_id_t                     ctx = 0;
  globus_result_t		   result;

  switch(argc) {
    case 5:
      objecttype = argv[1];
      object = argv[2];
      servicename = argv[3];
      action = argv[4];
      break;
    default:
      fprintf(stderr, USAGE, argv[0]);
      exit(1);
  }

  if (globus_module_activate(GLOBUS_GSI_AUTHZ_MODULE) != (int)GLOBUS_SUCCESS)
  {
      fprintf(stderr, "globus_module_activate failed\n");
      exit(1);
  }
  
  globus_gsi_authz_handle_init(&handle, servicename, ctx,
			       authtest_l_init_callback,
			       "init callback arg");

  result = globus_gsi_authorize(handle, action, object,
				authtest_l_authorize_callback,
				"authorize callback arg");
  if (result != GLOBUS_SUCCESS)
  {
      printf("authorize failed\n");
  }

  globus_module_deactivate(GLOBUS_GSI_AUTHZ_MODULE);

  exit(0);
}

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
