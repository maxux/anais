#ifndef __ANAIS_LIB_SERVER_H
	#define __ANAIS_LIB_SERVER_H
	
	typedef struct server_t {
		char *server;
		
	} server_t;
	
	void server_destruct(void *data);
	void server_new(char *name);	
	
	void server_server(char *data);
	void server_quit(server_t *server, char *data);
#endif
