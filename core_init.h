#ifndef __ANAIS_CORE_INIT_H
	#define __ANAIS_CORE_INIT_H
	
	#include <time.h>
	#include "core_ssl.h"
	
	#define MAXBUFF			4096
	#define SOCKET_TIMEOUT		10
	
	typedef struct codemap_t {
		char *filename;                /* library filename */
		void *handler;                 /* library handler */
		void (*main)(char *, char *);  /* main library function */
		void (*construct)(void);       /* pre-load data */
		void (*destruct)(void);        /* closing data, threads, ... */
		
	} codemap_t;
	
	typedef struct global_core_t {
		time_t startup_time;
		
		time_t rehash_time;
		unsigned int rehash_count;
		
		int sockfd;
		
		ssl_socket_t ssl;
		
	} global_core_t;
	
	extern ssl_socket_t *ssl;
	extern global_core_t global_core;
	
	int init_socket(char *server, int port);
	void raw_socket(char *message);
	int read_socket(ssl_socket_t *ssl, char *data, char *next);
	char *skip_server(char *data);
	
	void diep(char *str);
	
	void handle_private_message(char *data);
	
	int signal_intercept(int signal, void (*function)(int));
#endif
