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
#include <unistd.h>
#include <time.h>

#include "services.h"

#include "core_init.h"
#include "lib_database.h"
#include "lib_list.h"
#include "lib_core.h"
#include "lib_chan.h"
#include "lib_nick.h"
#include "lib_ircmisc.h"
#include "lib_module_nickserv.h"

void nick_dump() {
	list_node_t *node = global_lib.nicks->nodes;
	// channel_t *channel;
	
	while(node) {
		printf("[ ] DUMP == %s\n", node->name);
		node = node->next;
	}
}

umodes_t nick_umode(char mode) {
	// FIXME ?
	switch(mode) {
		case 'A': return UMODE_SERVER_ADMIN;
		case 'a': return UMODE_SERVICE_ADMIN;
		case 'B': return UMODE_BOT;
		case 'C': return UMODE_COADMIN;
		case 'd': return UMODE_CHPRIVMSG;
		case 'G': return UMODE_BADWORDS;
		case 'g': return UMODE_GL_LO_OPS;
		case 'H': return UMODE_HIDE_IRCOP;
		case 'h': return UMODE_AVAIL_HELP;
		case 'I': return UMODE_HIDE_OPER_IDLE;
		case 'i': return UMODE_INVISIBLE;
		case 'N': return UMODE_NET_ADMIN;
		case 'O': return UMODE_LOCAL_ADMIN;
		case 'o': return UMODE_GLOBAL_ADMIN;
		case 'p': return UMODE_HIDE_CHANS;
		case 'q': return UMODE_ULINES_KICK;
		case 'R': return UMODE_ALLOW_RMSG;
		case 'r': return UMODE_REGISTERED;
		case 'S': return UMODE_PDAEMONS;
		case 's': return UMODE_SRV_NOTICE;
		case 'T': return UMODE_DISABLE_CTCP;
		case 't': return UMODE_USE_VHOST;
		case 'V': return UMODE_WEBTV;
		case 'v': return UMODE_INFECT_DCC;
		case 'W': return UMODE_WHOIS_HIDDEN;
		case 'w': return UMODE_WALLOP;
		case 'x': return UMODE_HIDDEN_HOST;
		case 'z': return UMODE_USE_SSL;		
	}
	
	return 0;
}

umodes_t nick_umode_parse(char *modes, umodes_t original) {
	char signe = 0;
	
	while(*modes) {
		if(*modes == '+' || *modes == '-') {
			signe = *modes++;
			continue;
		}
		
		if(signe == '+')
			original |= nick_umode(*modes);
			
		else if(signe == '-')
			original &= ~nick_umode(*modes);
		
		modes++;
	}
	
	return original;
}

void nick_destruct(void *data) {
	nick_t *nick = (nick_t *) data;
	
	free(nick->nick);
	free(nick->user);
	free(nick->host);
	free(nick->vhost);
	free(nick->class);
	free(nick->fingerprint);
	free(nick);
}

void nick_new(char *request) {
	nick_t *nick;
	char *umodes;
	char trace[256];
	
	if(!(nick = (nick_t *) calloc(1, sizeof(nick_t))))
		diep("[-] malloc");
	
	nick->nick   = string_index(request, 0);
	nick->user   = string_index(request, 3);
	nick->host   = string_index(request, 4);
	nick->vhost  = string_index(request, 8);
	
	nick->channels = list_init(NULL);
		
	umodes = string_index(request, 7);
	nick->umodes = nick_umode_parse(umodes, nick->umodes);
	
	list_append(global_lib.nicks, (char *) nick->nick, nick);
	// list_dump(global_lib.nicks);
	
	// request TRACE for class name
	if(global_lib.sync) {
		zsnprintf(trace, ":" OPERSERV_NAME " TRACE %s", nick->nick);
		raw_socket(trace);
	}
}

void nick_away(nick_t *nick, char *data) {
	printf("[+] away: %s/%s\n", nick->nick, data);
}

void nick_change_callback(void *_channel, void *_oldnick, void *_newnick) {
	channel_t *channel = (channel_t *) _channel;
	char *oldnick, *newnick;
	nick_light_t *nicklight;
	cumodes_t modes;
	
	oldnick = (char *) _oldnick;
	newnick = (char *) _newnick;
	
	if(!(nicklight = list_search(channel->nicks, oldnick)))
		return;
	
	// saving umodes and clearing
	modes = nicklight->modes;
	list_remove(channel->nicks, oldnick);
	
	// creating new user
	nicklight = nick_light_new(newnick);
	nicklight->modes = modes;
	
	printf("[ ] nick_change/callback: appending %s to %s\n", newnick, channel->channel);
	list_append(channel->nicks, newnick, nicklight);
}

void nick_change(nick_t *nick, char *data) {
	char *newnick;
	allow_t allow;
	
	newnick = string_index(data, 0);
	
	printf("[+] nick: %s -> %s\n", nick->nick, newnick);
	
	// removing old name from list
	list_remove_only(global_lib.nicks, nick->nick);
	
	// changing on each channels
	list_iterate(global_lib.channels, nick_change_callback, nick->nick, newnick);
	
	// changing nick
	nick->nick    = newnick;
	nick->umodes &= ~UMODE_REGISTERED;
	nick->gid     = 0;
	
	// adding new one
	list_append(global_lib.nicks, newnick, nick);
	
	// checking if the newnick is not protected
	allow = nickserv_allow_protected(nick);
	nickserv_allow_check(nick, allow);
}

void nick_quit_callback(void *_channel, void *_nick, void *dummy) {
	(void) dummy;
	channel_t *channel = (channel_t *) _channel;
	
	printf("[+] quit: removing %s from %s\n", (char *) _nick, channel->channel);
	list_remove(channel->nicks, (char *) _nick);
}

void nick_quit(nick_t *nick, char *data) {
	(void) data;
	
	list_iterate(nick->channels, nick_quit_callback, nick->nick, NULL);
	
	printf("[+] quit: removing %s\n", nick->nick);
	list_remove(global_lib.nicks, nick->nick);
}

void nick_part(nick_t *nick, char *data) {
	char *chan;
	channel_t *channel;
	
	chan = string_index(data, 0);
	channel = list_search(global_lib.channels, chan);
	free(chan);

	printf("[+] part: removing %s from %s\n", nick->nick, channel->channel);
	list_remove(channel->nicks, nick->nick);
	list_remove(nick->channels, channel->channel);
	
	if(!channel->nicks->length) {
		printf("[+] part: no more user on %s, clearing it\n", channel->channel);
		list_remove(global_lib.channels, channel->channel);
	}
}

void nick_kick(nick_t *nick, char *data) {
	char *temp;
	channel_t *channel;
	nick_t *who;
	
	temp = string_index(data, 0);
	channel = list_search(global_lib.channels, temp);
	free(temp);
	
	temp = string_index(data, 1);
	who = list_search(global_lib.nicks, temp);
	free(temp);

	printf("[+] kick: %s kicked %s from %s\n", nick->nick, who->nick, channel->channel);
	list_remove(channel->nicks, who->nick);
	list_remove(who->channels, channel->channel);
	
	if(!channel->nicks->length) {
		printf("[+] part: no more user on %s, clearing it\n", channel->channel);
		list_remove(global_lib.channels, channel->channel);
	}
}

void nick_trace(server_t *server, char *data) {
	char *user;
	nick_t *nick;
	
	user = string_index(data, 3);
	
	if(!(nick = list_search(global_lib.nicks, user))) {
		fprintf(stderr, "[-] trace: cannot find user\n");
		free(user);
		return;
	}
	
	nick->class = string_index(data, 2);
	printf("[+] trace: %s: %s (%s)\n", server->server, nick->nick, nick->class);
	
	nickserv_fingerprint(nick);
}

void nick_trace_init_callback(void *_nick, void *d0, void *d1) {
	(void) d0;
	(void) d1;
	nick_t *nick = (nick_t *) _nick;
	char request[256];
	
	zsnprintf(request, ":" OPERSERV_NAME " TRACE %s", nick->nick);
	raw_socket(request);
}

void nick_trace_init() {
	// FIXME: crappy way
	raw_socket("SVSMODE " OPERSERV_NAME " +o");
	list_iterate(global_lib.nicks, nick_trace_init_callback, NULL, NULL);
}
