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
#include "lib_ircmisc.h"
#include "lib_module_chanserv.h"

const char *__cumodes_list = "ahoqv"; // admin, half, op, owner, voice
static const char *__skip_modes   = "befIjklL";

void nick_light_destruct(void *data) {
	nick_light_t *nicklight = (nick_light_t *) data;
	
	free(nicklight->nick);
	free(nicklight);
}

nick_light_t *nick_light_new(char *nick) {
	nick_light_t *nicklight;
	
	if(!(nicklight = (nick_light_t *) calloc(1, sizeof(nick_light_t))))
		diep("[-] calloc");
	
	nicklight->nick  = strdup(nick);
	nicklight->modes = 0;
	
	return nicklight;
}

cmodes_t chan_cmode(char mode) {
	// FIXME ?
	switch(mode) {
		case 'A': return CMODE_ADMIN_ONLY;
		case 'c': return CMODE_NO_ANSI_COLOR;
		case 'C': return CMODE_NO_CTCP;
		case 'o': return CMODE_INVITE_REQUIRED;
		case 'K': return CMODE_KNOCK_DENIED;
		case 'M': return CMODE_ONLY_REG_TALK;
		case 'm': return CMODE_MODERATED;
		case 'N': return CMODE_NO_NICK_CHANGE;
		case 'n': return CMODE_NO_MSG_OUTSIDE;
		case 'O': return CMODE_IRCOPS_ONLY;
		case 'p': return CMODE_PRIATE;
		case 'Q': return CMODE_ONLY_ULINE_KICK;
		case 'r': return CMODE_REGISTERED;
		case 'R': return CMODE_ONLY_REG_JOIN;
		case 'S': return CMODE_STRIP_COLOR;
		case 's': return CMODE_SECRET;
		case 't': return CMODE_HALFOP_TOPIC;
		case 'T': return CMODE_NO_NOTICE;
		case 'u': return CMODE_AUDITORIUM;
		case 'V': return CMODE_NO_INVITE;
		case 'z': return CMODE_SSL_ONLY;
		case 'Z': return CMODE_ALL_SSL;
	}
	
	return 0;
}

char *chan_cmode_text(cmodes_t cmode) {
	char temp[256], *flag;
	
	flag = temp;
	*flag++ = '+';
	
	if(cmode & CMODE_ADMIN_ONLY)      *flag++ = 'A'; 
	if(cmode & CMODE_NO_ANSI_COLOR)   *flag++ = 'c';
	if(cmode & CMODE_NO_CTCP)         *flag++ = 'C';
	if(cmode & CMODE_INVITE_REQUIRED) *flag++ = 'o';
	if(cmode & CMODE_KNOCK_DENIED)    *flag++ = 'K';
	if(cmode & CMODE_ONLY_REG_TALK)   *flag++ = 'M';
	if(cmode & CMODE_MODERATED)       *flag++ = 'm';
	if(cmode & CMODE_NO_NICK_CHANGE)  *flag++ = 'N';
	if(cmode & CMODE_NO_MSG_OUTSIDE)  *flag++ = 'n';
	if(cmode & CMODE_IRCOPS_ONLY)     *flag++ = 'O';
	if(cmode & CMODE_PRIATE)          *flag++ = 'p';
	if(cmode & CMODE_ONLY_ULINE_KICK) *flag++ = 'Q';
	if(cmode & CMODE_REGISTERED)      *flag++ = 'r';
	if(cmode & CMODE_ONLY_REG_JOIN)   *flag++ = 'R';
	if(cmode & CMODE_STRIP_COLOR)     *flag++ = 'S';
	if(cmode & CMODE_SECRET)          *flag++ = 's';
	if(cmode & CMODE_HALFOP_TOPIC)    *flag++ = 't';
	if(cmode & CMODE_NO_NOTICE)       *flag++ = 'T';
	if(cmode & CMODE_AUDITORIUM)      *flag++ = 'u';
	if(cmode & CMODE_NO_INVITE)       *flag++ = 'V';
	if(cmode & CMODE_SSL_ONLY)        *flag++ = 'z';
	if(cmode & CMODE_ALL_SSL)         *flag++ = 'Z';
	*flag = '\0';

	return strdup(temp);
}

cumodes_t chan_cumode(char mode) {
	// FIXME ?
	switch(mode) {
		case 'a': return CMODE_USER_ADMIN;
		case 'h': return CMODE_USER_HALFOP;
		case 'o': return CMODE_USER_OPERATOR;
		case 'q': return CMODE_USER_OWNER;
		case 'v': return CMODE_USER_VOICE;
	}
	
	return 0;
}

char *chan_cumode_text(cumodes_t cumode) {
	char temp[256], *flag;
	
	flag = temp;
	*flag++ = '+';
	
	if(cumode & CMODE_USER_ADMIN)    *flag++ = 'a';
	if(cumode & CMODE_USER_HALFOP)   *flag++ = 'h';
	if(cumode & CMODE_USER_OPERATOR) *flag++ = 'o';
	if(cumode & CMODE_USER_OWNER)    *flag++ = 'q';
	if(cumode & CMODE_USER_VOICE)    *flag++ = 'v';
	*flag = '\0';

	return strdup(temp);
}

void chan_cmode_change(cmodes_t *cmode, char signe, cmodes_t mode) {
	if(signe == '+')
		*cmode |= mode;
				
	else if(signe == '-')
		*cmode &= ~mode;
}

void chan_cumode_change(cumodes_t *cumode, char signe, cumodes_t mode) {
	if(signe == '+')
		*cumode |= mode;
				
	else if(signe == '-')
		*cumode &= ~mode;
}

cmodes_t chan_cmode_parse(channel_t *channel, char *modes, char *argv[]) {
	nick_light_t *nicklight;
	char signe = 0;
	int i = 0;
	
	for(; *modes; modes++) {
		if(*modes == '+' || *modes == '-') {
			signe = *modes;
			continue;
		}
		
		// FIXME: handle it, n00b
		if(strchr(__skip_modes, *modes)) {
			i++;
			continue;
		}
		
		// if user/arguments
		if(strchr(__cumodes_list, *modes)) {
			printf("[ ] cmode_parse: argc %d, argv: <%s>\n", i, argv[i]);
			
			if(!(nicklight = list_search(channel->nicks, argv[i++]))) {
				// should not happen
				fprintf(stderr, "[-] nick not found\n");
				continue;
			}
			
			chan_cumode_change(&nicklight->modes, signe, chan_cumode(*modes));
			
		} else chan_cmode_change(&channel->cmodes, signe, chan_cmode(*modes));
	}
	
	return channel->cmodes;
}

// check if a mode is already set on channel->user
int chan_nickmode_isset(channel_t *channel, char *nick, char mode) {
	nick_light_t *nicklight;
	
	if(!(nicklight = list_search(channel->nicks, nick))) {
		fprintf(stderr, "[-] nick not found\n");
		return 0;
	}
	
	return nicklight->modes & chan_cumode(mode);
}

// change (multiple) modes on same user
void chan_cmode_single_edit(channel_t *channel, char *flags, char *user) {
	char *argv[256], list[2048], temp[2048];
	unsigned int argc, i;
	
	chan_dump(channel->channel);
	
	*list = '\0';
	printf("[ ] cmode: user: %s\n", user);
	
	for(i = 1, argc = 0; i <= strlen(flags) - 1; i++) {
		// set flags to list if not already set
		if(strchr(__cumodes_list, flags[i]) && !chan_nickmode_isset(channel, user, flags[i])) {
			printf("[ ] cmode: appending %s for %c (argc: %d)\n", user, flags[i], argc);
			argv[argc++] = user;
			
			strcat(list, user);
			strcat(list, " ");
		}
	}
	
	if(!*list)
		return;
	
	zsnprintf(temp, "MODE %s %s %s", channel->channel, flags, list);
	raw_socket(temp);
		
	chan_cmode_parse(channel, flags, argv);
	chan_dump(channel->channel);
}

void chan_apply_topic(channel_t *channel) {
	char request[1024];
	
	zsnprintf(request, "TOPIC %s :%s", channel->channel, channel->topic.topic);
	raw_socket(request);
}

void chan_destruct(void *data) {
	channel_t *channel = (channel_t *) data;
	
	free(channel->channel);
	list_free(channel->nicks);
	
	free(channel);
}

void chan_new(char *request) {
	channel_t *channel;
	
	printf("[+] chan/new: creating channel %s\n", request);
	
	if(!(channel = (channel_t *) calloc(1, sizeof(channel_t))))
		diep("[-] calloc");
	
	channel->channel = strdup(request);
	channel->cmodes  = 0;
	channel->nicks   = list_init(nick_light_destruct);
	
	list_append(global_lib.channels, (char *) request, channel);
	// list_dump(global_lib.channels);
	
	// loading data from database if exists
	if(chanserv_chan_exists(channel->channel)) {
		// skipping reset during load
		if(global_lib.sync) {
			printf("[+] chan/new: loading saved state\n");
			chanserv_load(channel);
		}
	}
}

void chan_joining(nick_t *nick, char *chan) {
	sqlite3_stmt *stmt;
	char *sqlquery, *flags;
	channel_t *channel;
	nick_light_t *nicklight;
	
	printf("[+] joining: %s/%s\n", nick->nick, chan);
	
	// WARNING
	while(!(channel = list_search(global_lib.channels, chan))) {
		printf("[-] joining: channel not found, creating it\n");
		chan_new(chan);
	}
	
	// appending channel on user channels list
	list_append(nick->channels, channel->channel, channel);
	
	nicklight = nick_light_new(nick->nick);
	list_append(channel->nicks, (char *) nick->nick, nicklight);
	
	// list_dump(channel->nicks);
	
	if(chanserv_chan_exists(chan)) {
		chan_cmode_single_edit(channel, "+r", NULL);
	}
	
	// not identified, skipping
	if(!(nick->umodes & UMODE_REGISTERED))
		return;
	
	// skipping load time
	if(!global_lib.sync)
		return;
	
	// checking access	
	sqlquery = sqlite3_mprintf(
		"SELECT flags FROM cs_access c, ns_nick n "
		"WHERE UPPER(c.channel) = UPPER('%q')     "
		"  AND UPPER(n.nick)    = UPPER('%q')     "
		"  AND n.gid = c.gid                      ",
		chan, nick->nick
	);
	
	if((stmt = db_sqlite_select_query(sqlite_db, sqlquery))) {
		if(sqlite3_step(stmt) == SQLITE_ROW) {
			flags = (char *) sqlite3_column_text(stmt, 0);
			chan_cmode_single_edit(channel, flags, nick->nick);
		}
	
	} else fprintf(stderr, "[-] chanserv/joining: cannot select\n");
	
	sqlite3_free(sqlquery);
	sqlite3_finalize(stmt);
}

void chan_join(nick_t *nick, char *data) {
	char *channel, *match;
	
	printf("[+] join: %s/%s\n", nick->nick, data);
	
	channel = data;
	while((match = strchr(data, ','))) {
		channel = strndup(data, match - data);
		data = match + 1;
		
		chan_joining(nick, channel);
		free(channel);
	}
	
	chan_joining(nick, data);
}

void chan_mode(char *data) {
	char *modes, *chan, *argv[256];
	channel_t *channel;
	int argc = 0;
	
	printf("[ ] mode: %s\n", data);
	
	// grabbing modes list
	modes = string_index(data, 1);
	
	// skipping timestamp
	if(!strcmp(modes, "+")) {
		free(modes);
		return;
	}
	
	// grabbing channel
	chan = string_index(data, 0);
	if(!(channel = list_search(global_lib.channels, chan))) {
		// should not happen
		fprintf(stderr, "[-] channel <%s> not found on list\n", chan);
		
		free(chan);
		free(modes);
		return;
	}
	
	free(chan);
	
	// building arguments list
	while((argv[argc] = string_index(data, argc + 2)))
		argc++;
	
	chan_cmode_parse(channel, modes, argv);
	
	// freeing arguments list
	while(argc)
		free(argv[--argc]);
	
	free(modes);
}

// channels mode wrapper
void chan_mode_server(server_t *server, char *data) {
	(void) server;
	chan_mode(data);
}

// channels mode wrapper
void chan_mode_user(nick_t *nick, char *data) {
	(void) nick;
	channel_t *channel;
	char *chan;
	
	// calling parent
	chan_mode(data);
	
	// saving changes
	chan = string_index(data, 0);
	channel = list_search(global_lib.channels, chan);
	free(chan);
	
	if(channel->cmodes & CMODE_REGISTERED)
		chan_save_flags(channel);
}

void chan_save_flags(channel_t *channel) {
	char *sqlquery, *flags;
	
	flags = chan_cmode_text(channel->cmodes);
	
	sqlquery = sqlite3_mprintf(
		"UPDATE cs_channel SET flags = '%q' WHERE channel = '%q'", 
		flags, channel->channel
	);
	
	if(!db_sqlite_simple_query(sqlite_db, sqlquery))
		fprintf(stderr, "[-] chan/save_flags: cannot save flags\n");
	
	free(flags);
	sqlite3_free(sqlquery);
}

void chan_topic(char *data) {
	char *chan, *sqlquery, *timestamp;
	channel_t *channel;
	
	chan = string_index(data, 0);
	if(!(channel = list_search(global_lib.channels, chan))) {
		fprintf(stderr, "[-] topic: channel not found\n");
		free(chan);
		return;
	}
	
	free(chan);
	
	channel->topic.author = string_index(data, 1);
	printf(">> %s\n", channel->topic.author);
	
	timestamp = string_index(data, 2);
	channel->topic.updated = atoi(timestamp);
	free(timestamp);
	printf("[+] topic: timestamp: %ld\n", channel->topic.updated);
	
	channel->topic.topic = strdup(strchr(data, ':') + 1);
	
	printf("[+] topic %s/%s\n", channel->channel, channel->topic.topic);
	
	if(channel->cmodes & CMODE_REGISTERED) {
		sqlquery = sqlite3_mprintf(
			"UPDATE cs_channel SET topic = '%q', topic_author = '%q' "
			"WHERE channel = '%q'",
			channel->topic.topic, channel->topic.author, channel->channel
		);
		
		// creating group
		if(!db_sqlite_simple_query(sqlite_db, sqlquery))
			fprintf(stderr, "[-] cannot save topic");
		
		sqlite3_free(sqlquery);
	}
}

void chan_topic_server(server_t *server, char *data) {
	(void) server;
	chan_topic(data);
}

void chan_topic_user(nick_t *nick, char *data) {
	(void) nick;
	chan_topic(data);
}

void chan_dump_iter(void *_nicklight, void *d1, void *d2) {
	(void) d1;
	(void) d2;
	nick_light_t *nicklight = (nick_light_t *) _nicklight;
	cumodes_t cumode = nicklight->modes;
	
	if(cumode & CMODE_USER_ADMIN)    printf("[ ] Nick %s -> mode a sets\n", nicklight->nick);
	if(cumode & CMODE_USER_HALFOP)   printf("[ ] Nick %s -> mode h sets\n", nicklight->nick);
	if(cumode & CMODE_USER_OPERATOR) printf("[ ] Nick %s -> mode o sets\n", nicklight->nick);
	if(cumode & CMODE_USER_OWNER)    printf("[ ] Nick %s -> mode q sets\n", nicklight->nick);
	if(cumode & CMODE_USER_VOICE)    printf("[ ] Nick %s -> mode v sets\n", nicklight->nick);
}

void chan_dump(char *chan) {
	channel_t *channel;
	cmodes_t cmode;
	
	if(!(channel = list_search(global_lib.channels, chan))) {
		fprintf(stderr, "[-] dump: channel not found\n");
		return;
	}
	
	cmode = channel->cmodes;
	
	if(cmode & CMODE_ADMIN_ONLY)      printf("[ ] Channel %s -> mode A sets\n", channel->channel); 
	if(cmode & CMODE_NO_ANSI_COLOR)   printf("[ ] Channel %s -> mode c sets\n", channel->channel);
	if(cmode & CMODE_NO_CTCP)         printf("[ ] Channel %s -> mode C sets\n", channel->channel);
	if(cmode & CMODE_INVITE_REQUIRED) printf("[ ] Channel %s -> mode o sets\n", channel->channel);
	if(cmode & CMODE_KNOCK_DENIED)    printf("[ ] Channel %s -> mode K sets\n", channel->channel);
	if(cmode & CMODE_ONLY_REG_TALK)   printf("[ ] Channel %s -> mode M sets\n", channel->channel);
	if(cmode & CMODE_MODERATED)       printf("[ ] Channel %s -> mode m sets\n", channel->channel);
	if(cmode & CMODE_NO_NICK_CHANGE)  printf("[ ] Channel %s -> mode N sets\n", channel->channel);
	if(cmode & CMODE_NO_MSG_OUTSIDE)  printf("[ ] Channel %s -> mode n sets\n", channel->channel);
	if(cmode & CMODE_IRCOPS_ONLY)     printf("[ ] Channel %s -> mode O sets\n", channel->channel);
	if(cmode & CMODE_PRIATE)          printf("[ ] Channel %s -> mode p sets\n", channel->channel);
	if(cmode & CMODE_ONLY_ULINE_KICK) printf("[ ] Channel %s -> mode Q sets\n", channel->channel);
	if(cmode & CMODE_REGISTERED)      printf("[ ] Channel %s -> mode r sets\n", channel->channel);
	if(cmode & CMODE_ONLY_REG_JOIN)   printf("[ ] Channel %s -> mode R sets\n", channel->channel);
	if(cmode & CMODE_STRIP_COLOR)     printf("[ ] Channel %s -> mode S sets\n", channel->channel);
	if(cmode & CMODE_SECRET)          printf("[ ] Channel %s -> mode s sets\n", channel->channel);
	if(cmode & CMODE_HALFOP_TOPIC)    printf("[ ] Channel %s -> mode t sets\n", channel->channel);
	if(cmode & CMODE_NO_NOTICE)       printf("[ ] Channel %s -> mode T sets\n", channel->channel);
	if(cmode & CMODE_AUDITORIUM)      printf("[ ] Channel %s -> mode u sets\n", channel->channel);
	if(cmode & CMODE_NO_INVITE)       printf("[ ] Channel %s -> mode V sets\n", channel->channel);
	if(cmode & CMODE_SSL_ONLY)        printf("[ ] Channel %s -> mode z sets\n", channel->channel);
	if(cmode & CMODE_ALL_SSL)         printf("[ ] Channel %s -> mode Z sets\n", channel->channel);
	
	list_iterate(channel->nicks, chan_dump_iter, NULL, NULL);
}
