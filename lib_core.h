#ifndef __ANAIS_LIB_CORE_H
	#define __ANAIS_LIB_CORE_H
	
	#include <time.h>
	#include <string.h>
	#include "lib_list.h"
	#include "lib_server.h"
	#include "lib_nick.h"
	
	#define NICKSERV_NAME    "nickserv"
	#define CHANSERV_NAME    "chanserv"
	#define OPERSERV_NAME    "operserv"
	
	typedef struct modules_t {
		char *name;
		void (*callback)(nick_t *, char *);
		
	} modules_t;
	
	typedef struct request_t {
		char *name;
		void (*ucb)(nick_t *, char *);    // user callback
		void (*scb)(server_t *, char *);  // server callback
		
	} request_t;
	
	typedef struct global_lib_t {
		struct list_t *servers;
		struct list_t *channels;
		struct list_t *nicks;
		char sync;
		
	} global_lib_t;
	
	/* Global Data */
	extern global_lib_t global_lib;
	
	void main_construct(void);
	void main_core(char *data, char *request);
	void main_destruct(void);
	
	void handle_query(nick_t *nick, char *data);
	
	#define zsnprintf(x, ...) snprintf(x, sizeof(x), __VA_ARGS__)
	
	typedef void (*user_callback)(nick_t *, char *);
	typedef void (*server_callback)(server_t *, char *);
	
	void mode_dispatch(nick_t *nick, char *data);
	
	void irc_privmsg(char *dest, char *message);
	void irc_notice(char *user, char *message);
	void irc_noticefrom(char *from, char *user, char *message);
#endif
