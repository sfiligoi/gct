#include "myproxy_common.h"	/* all needed headers included here */

static int myproxy_authorize_init(myproxy_socket_attrs_t *attrs,
                                  char *passphrase,
				  char *certfile);

int myproxy_set_delegation_defaults(
    myproxy_socket_attrs_t *socket_attrs,
    myproxy_request_t      *client_request)
{ 
    char *username, *pshost;

    client_request->version = strdup(MYPROXY_VERSION);
    client_request->command_type = MYPROXY_GET_PROXY;

    username = getenv("LOGNAME");
    if (username != NULL) {
      client_request->username = strdup(username);
    }

    pshost = getenv("MYPROXY_SERVER");
    if (pshost != NULL) {
        socket_attrs->pshost = strdup(pshost);
    }

    client_request->proxy_lifetime = 60*60*MYPROXY_DEFAULT_DELEG_HOURS;

    socket_attrs->psport = MYPROXY_SERVER_PORT;

    return 0;
}
    
int myproxy_get_delegation(
myproxy_socket_attrs_t *socket_attrs,
    myproxy_request_t      *client_request,
    char *certfile,
    myproxy_response_t     *server_response,
    char *outfile)

{    
    char delegfile[128];
    char request_buffer[2048];
    int  requestlen;

    /* Set up client socket attributes */
    if (myproxy_init_client(socket_attrs) < 0) {
        fprintf(stderr, "Error: %s\n", verror_get_string());
        return(1);
    }
    
    /* Attempt anonymous-mode credential retrieval if we don't have a
       credential. */
    GSI_SOCKET_allow_anonymous(socket_attrs->gsi_socket, 1);

     /* Authenticate client to server */
    if (myproxy_authenticate_init(socket_attrs, NULL) < 0) {
        fprintf(stderr, "Error: %s: %s\n", 
		socket_attrs->pshost, verror_get_string());
        return(1);
    }

    /* Serialize client request object */
    requestlen = myproxy_serialize_request(client_request, 
                                           request_buffer, sizeof(request_buffer));
    if (requestlen < 0) {
        fprintf(stderr, "Error in myproxy_serialize_request():\n");
        return(1);
    }

    /* Send request to the myproxy-server */
    if (myproxy_send(socket_attrs, request_buffer, requestlen) < 0) {
        fprintf(stderr, "Error in myproxy_send_request(): %s\n", 
		verror_get_string());
        return(1);
    }

    /* Continue unless the response is not OK */
    if (myproxy_authorize_init(socket_attrs, client_request->passphrase,
	                       certfile) < 0) {
	  fprintf(stderr, "%s\n",
	          verror_get_string());
	  return(1);
    }

    /* Accept delegated credentials from server */
    if (myproxy_accept_delegation(socket_attrs, delegfile, sizeof(delegfile), NULL) < 0) {
        fprintf(stderr, "Error in myproxy_accept_delegation(): %s\n", 
		verror_get_string());
	return(1);
    }      

    if (myproxy_recv_response(socket_attrs, server_response) < 0) {
       fprintf(stderr, "%s\n", verror_get_string());
       return(1);
    }

    /* move delegfile to outputfile if specified */
    if (outfile != NULL) {
        if (copy_file(delegfile, outfile, 0600) < 0) {
	    fprintf(stderr, "Error creating file: %s\n", outfile);
	    return(1);
	}
	ssl_proxy_file_destroy(delegfile);
    }

    return(0);
}

static int
myproxy_authorize_init(myproxy_socket_attrs_t *attrs,
                       char *passphrase,
		       char *certfile)
{
   myproxy_response_t *server_response = NULL;
   myproxy_proto_response_type_t response_type;
   authorization_data_t *d; 
   /* just pointer into server_response->authorization_data, no memory is 
      allocated for this pointer */
   int return_status = -1;
   char buffer[8192];
   int bufferlen;

   do {
      server_response = malloc(sizeof(*server_response));
      memset(server_response, 0, sizeof(*server_response));
      if (myproxy_recv_response(attrs, server_response) < 0) {
	 goto end;
      }

      response_type = server_response->response_type;
      if (response_type == MYPROXY_AUTHORIZATION_RESPONSE) {
	 if (certfile == NULL)
	    d = authorization_create_response(
		              server_response->authorization_data,
			      AUTHORIZETYPE_PASSWD,
			      passphrase,
			      strlen(passphrase) + 1);
	 else 
	    d = authorization_create_response(
		               server_response->authorization_data,
			       AUTHORIZETYPE_CERT,
			       certfile,
			       strlen(certfile) + 1);
	 if (d == NULL) {
	    verror_put_string("Cannot create authorization response");
       	    goto end;
	 }

	 if (d->client_data_len + sizeof(int) > sizeof(buffer)) {
	       verror_put_string("Internal buffer too small");
	       goto end;
	 }
	 (*buffer) = d->method;
	 bufferlen = d->client_data_len + sizeof(int);

	 memcpy(buffer + sizeof(int), d->client_data, d->client_data_len);
	 /* Send the authorization data to the server */
	 if (myproxy_send(attrs, buffer, bufferlen) < 0) {
	    goto end;
	 }
      }
      myproxy_free(NULL, NULL, server_response);
      server_response = NULL;
   } while (response_type == MYPROXY_AUTHORIZATION_RESPONSE);

   return_status = 0;
end:
   myproxy_free(NULL, NULL, server_response);

   return return_status;
}
