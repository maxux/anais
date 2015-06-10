/* anais - irc services
 * Author: Daniel Maxime (root@maxux.net)
 * Contributor: Darky, mortecouille, RaphaelJ, somic, ghilan
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <netdb.h>
#include <curl/curl.h>
#include <ctype.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>

#include "services.h"

#include "core_init.h"
#include "lib_database.h"
#include "lib_list.h"
#include "lib_ircmisc.h"
#include "lib_core.h"
#include "lib_nick.h"
#include "lib_chan.h"
#include "lib_server.h"
#include "lib_admin.h"

#include "lib_module_operserv.h"
#include "lib_module_nickserv.h"
#include "lib_module_chanserv.h"

global_lib_t global_lib = {
	.servers  = NULL,
	.channels = NULL,
	.nicks    = NULL,
	.sync     = 0,
};

modules_t __modules[] = {
	{.name = OPERSERV_NAME, .callback = operserv_query},
	{.name = NICKSERV_NAME, .callback = nickserv_query},
	{.name = CHANSERV_NAME, .callback = chanserv_query},
};

request_t __request[] = {
	// user
	{.name = "PRIVMSG",     .ucb = handle_query},
	{.name = "AWAY",        .ucb = nick_away},
	{.name = "JOIN",        .ucb = chan_join},
	{.name = "NICK",        .ucb = nick_change},
	{.name = "QUIT",        .ucb = nick_quit},
	{.name = "PART",        .ucb = nick_part},
	{.name = "KICK",        .ucb = nick_kick},
	{.name = "TOPIC",       .ucb = chan_topic_user,
	                        .scb = chan_topic_server},
	
	// server
	{.name = "MODE",        .ucb = mode_dispatch,
	                        .scb = chan_mode_server},
	{.name = "205",         .scb = nick_trace},
	{.name = "204",         .scb = nick_trace},
};

/* Signals */
void lib_sighandler(int signal) {
	switch(signal) {
		case SIGUSR1:
		break;
	}
}

/*
 * Chat Handling
 */
void irc_privmsg(char *dest, char *message) {
	char buffer[2048];
	
	zsnprintf(buffer, "PRIVMSG %s :%s", dest, message);
	raw_socket(buffer);
}

void irc_notice(char *user, char *message) {
	char buffer[2048];
	
	zsnprintf(buffer, "NOTICE %s :%s", user, message);
	raw_socket(buffer);
}

void irc_noticefrom(char *from, char *user, char *message) {
	char buffer[2048];
	
	zsnprintf(buffer, ":%s NOTICE %s :%s", from, user, message);
	raw_socket(buffer);
}

void irc_fakeuser(char *nickname) {
	char request[1024];
	
	zsnprintf(request, "NICK %s 1 %ld services %s %s %s +r :Service",
	                   nickname, time(NULL), IRC_LOCAL_SERVER,
	                   IRC_LOCAL_SERVER, nickname);
	                   
	raw_socket(request);
	
	zsnprintf(request, "SQLINE %s :Reserved for services", nickname);
	raw_socket(request);
}

void handle_query(nick_t *nick, char *data) {
	char *modrequest = NULL;
	unsigned int i;
	
	modrequest = strtolower(string_index(data, 0));
	
	for(i = 0; i < sizeof(__modules) / sizeof(modules_t); i++) {
		printf("[ ] core: query: %s <> %s\n", __modules[i].name, modrequest);
		
		if(!strcmp(__modules[i].name, modrequest))
			__modules[i].callback(nick, strchr(data + 1, ':') + 1);
	}
	
	free(modrequest);
	
	return;
}

void wrapper_action(char *data, user_callback ucb, server_callback scb) {
	char *request = NULL, *temp = NULL;
	char *useme;
	nick_t *nick;
	server_t *server;
	
	request = string_index(data, 1);
	temp    = string_index(data, 0);
	
	// nick action	
	if((nick = list_search(global_lib.nicks, temp + 1))) {
		useme = strstr(data, request) + strlen(request) + 1;
		ucb(nick, useme);
	
	// server action
	} else if((server = list_search(global_lib.servers, temp + 1))) {
		useme = strstr(data, request) + strlen(request) + 1;
		scb(server, useme);
		
	} else fprintf(stderr, "[-] wrapper_action: user/server (%s) not found\n", temp + 1);
	
	free(temp);
	free(request);
}

void mode_dispatch(nick_t *nick, char *data) {
	char *modes;
	
	// user
	if(*data != '#') {
		modes = string_index(data, 1);
		printf("[ ] mode_dispatch: previous usermodes: %x\n", nick->umodes);
		nick->umodes = nick_umode_parse(modes + 1, nick->umodes);
		printf("[ ] mode_dispatch: new usermodes: %x\n", nick->umodes);
		
	// channel
	} else chan_mode_user(nick, data);
}

void main_core(char *data, char *request) {
	unsigned int i;

	// raw data part
	if(!strncmp(data, "PING", 4)) {
		data[1] = 'O';		/* pOng */
		raw_socket(data);
		return;
	}
	
	if(!strncmp(data, "NETINFO", 7)) {
		for(i = 0; i < sizeof(__modules) / sizeof(modules_t); i++)
			irc_fakeuser(__modules[i].name);
		
		// FIXME: hack for TRACE command
		global_lib.sync = 1;
		nick_trace_init();
		
		return;
	}
	
	if(!strncmp(data, "NICK", 4)) {
		nick_new(data + 5);
		return;
	}
	
	if(!strncmp(data, "TOPIC", 5)) {
		chan_topic(data + 6);
		return;
	}
	
	// special case
	if(!strncmp(data, "SERVER", 6)) {
		server_server(data + 6);
		return;
	}
	
	// request part
	for(i = 0; i < sizeof(__request) / sizeof(request_t); i++) {
		if(!strncmp(__request[i].name, request, strlen(__request[i].name)))
			wrapper_action(data, __request[i].ucb, __request[i].scb);
	}
}

void main_construct(void) {
	// init lists
	global_lib.servers  = list_init(server_destruct);
	global_lib.channels = list_init(chan_destruct);
	global_lib.nicks    = list_init(nick_destruct);
	
	// opening sqlite
	sqlite_db = db_sqlite_init();
	
	// grabbing SIGUSR1 for daily update
	signal_intercept(SIGUSR1, lib_sighandler);
}

void main_destruct(void) {
	// closing sqitite
	db_sqlite_close(sqlite_db);
}
