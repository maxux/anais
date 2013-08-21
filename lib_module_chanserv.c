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

#include "services.h"

#include "core_init.h"
#include "lib_database.h"
#include "lib_list.h"
#include "lib_core.h"
#include "lib_nick.h"
#include "lib_chan.h"
#include "lib_ircmisc.h"
#include "lib_module_nickserv.h"
#include "lib_module_chanserv.h"

#define irc_notice(...) irc_noticefrom(CHANSERV_NAME, __VA_ARGS__)

request_t __chanserv[] = {
	{.name = "register",    .ucb = chanserv_register},
	{.name = "access" ,     .ucb = chanserv_access},
	{.name = "info" ,       .ucb = chanserv_info},
	{.name = "help",        .ucb = chanserv_help},
};

char *chanserv_passwd(char *chan, char *password) {
	char hashme[512];
	
	zsnprintf(hashme, "%s%s", chan, password);
	return md5ascii(hashme);
}

int chanserv_args_check(nick_t *nick, char *data, int argc, char *syntax) {
	char answer[1024];
	
	if(words_count(data) < (unsigned int) argc) {
		zsnprintf(answer, "Missing arguments: %s", syntax);
		irc_notice(nick->nick, answer);
		return 1;
	}
	
	return 0;
}

int chanserv_chan_exists(char *chan) {
	sqlite3_stmt *stmt;
	char *sqlquery;
	int retcode = 0;
	
	sqlquery = sqlite3_mprintf(
		"SELECT COUNT(*) FROM cs_channel WHERE UPPER(channel) = UPPER('%q')",
		chan
	);
	
	if((stmt = db_sqlite_select_query(sqlite_db, sqlquery))) {
		while(sqlite3_step(stmt) == SQLITE_ROW)
			retcode = sqlite3_column_int(stmt, 0);
	
	} else fprintf(stderr, "[-] chanserv/chan_exists: cannot select\n");
	
	sqlite3_free(sqlquery);
	sqlite3_finalize(stmt);
	
	return retcode;
}

void chanserv_register(nick_t *nick, char *data) {
	channel_t *channel;
	nick_light_t *nicklight;
	char *sqlquery, *password;
	char *hashed, *chan;
	
	if(chanserv_args_check(nick, data, 3, "REGISTER #channel password"))
		return;
	
	// check if user is identified
	if(!(nick->umodes & UMODE_REGISTERED)) {
		irc_notice(nick->nick, "You are not identified");
		return;
	}
	
	// grabbing chan
	chan = string_index(data, 1);
	if(!(channel = list_search(global_lib.channels, chan))) {
		irc_notice(nick->nick, "Channel doesn't exist");
		free(chan);
		return;
	}
	
	free(chan);
	
	// check if channel is not already registred
	if(channel->cmodes & CMODE_REGISTERED) {
		irc_notice(nick->nick, "Channel already registered");
		return;
	}
	
	// check if the user is on the channel
	if(!(nicklight = list_search(channel->nicks, nick->nick))) {
		irc_notice(nick->nick, "You are not on this channel");
		return;
	}
	
	// check if the user is operator
	if(!(nicklight->modes & CMODE_USER_OPERATOR)) {
		irc_notice(nick->nick, "You are not operator on this channel");
		return;
	}
	
	// hash password
	password = string_index(data, 2);
	hashed   = chanserv_passwd(channel->channel, password);
	
	sqlquery = sqlite3_mprintf(
		"INSERT INTO cs_channel (channel, topic, owner, password, rdate) "
		"VALUES ('%q', '%q', '%q', '%s', datetime('now', 'localtime'))",
		channel->channel, channel->topic.topic, nick->nick, hashed
	);
	
	// creating group
	if(db_sqlite_simple_query(sqlite_db, sqlquery)) {
		sqlite3_free(sqlquery);
		
		// creating nick
		sqlquery = sqlite3_mprintf(
			"INSERT INTO cs_access (channel, gid, flags) VALUES "
			"('%q', (SELECT gid FROM ns_nick WHERE nick = '%q'), '+qo')",
			channel->channel, nick->nick
		);
		
		if(db_sqlite_simple_query(sqlite_db, sqlquery)) {
			irc_notice(nick->nick, "Channel now registered");
			
		} else irc_notice(nick->nick, "Cannot set default access");
		
		chan_cmode_single_edit(channel, "+rq", nick->nick);
		
		// saving channel data
		chan_save_flags(channel);
		
		if(!db_sqlite_simple_query(sqlite_db, sqlquery))
			fprintf(stderr, "[-] chanserv/register: cannot save flags\n");
		
	} else irc_notice(nick->nick, "Cannot register the channel");
	
	sqlite3_free(sqlquery);
	
	free(hashed);
	free(password);
}

void chanserv_load(channel_t *channel) {
	sqlite3_stmt *stmt;
	char *sqlquery, *topic, *flags;
	char temp[1024];
	
	printf("[+] chanserv: loading %s\n", channel->channel);
	
	sqlquery = sqlite3_mprintf(
		"SELECT topic, flags FROM cs_channel WHERE UPPER(channel) = UPPER('%q')",
		channel->channel
	);
	
	if((stmt = db_sqlite_select_query(sqlite_db, sqlquery))) {
		while(sqlite3_step(stmt) == SQLITE_ROW) {
			topic = (char *) sqlite3_column_text(stmt, 0);
			flags = (char *) sqlite3_column_text(stmt, 1);
			
			if(topic) {
				zsnprintf(temp, "TOPIC %s :%s", channel->channel, topic);
				raw_socket(temp);
			}
			
			if(flags) {
				zsnprintf(temp, "MODE %s %s", channel->channel, flags);
				raw_socket(temp);
				
				chan_cmode_parse(channel, flags, NULL);
			}
		}
	
	} else fprintf(stderr, "[-] chanserv/load: cannot select\n");
	
	sqlite3_free(sqlquery);
	sqlite3_finalize(stmt);
}

void chanserv_access_show(channel_t *channel, nick_t *nick) {
	sqlite3_stmt *stmt;
	char *sqlquery, *who, *flags;
	char temp[1024];
	
	printf("[+] chanserv: loading %s access\n", channel->channel);
	
	sqlquery = sqlite3_mprintf(
		"SELECT n.nick, c.flags FROM cs_access c, ns_nick n "
		"WHERE UPPER(c.channel) = UPPER('%q')               "
		"  AND n.gid = c.gid                                ",
		channel->channel
	);
	
	if((stmt = db_sqlite_select_query(sqlite_db, sqlquery))) {
		while(sqlite3_step(stmt) == SQLITE_ROW) {
			who   = (char *) sqlite3_column_text(stmt, 0);
			flags = (char *) sqlite3_column_text(stmt, 1);
			
			zsnprintf(temp, "%-25s %s", who, flags);
			irc_notice(nick->nick, temp);
		}
	
	} else fprintf(stderr, "[-] chanserv/access_show: cannot select\n");
	
	sqlite3_free(sqlquery);
	sqlite3_finalize(stmt);
}

void chanserv_access(nick_t *nick, char *data) {
	char *chan, *user, *flags, *testme;
	char *sqlquery;
	channel_t *channel;
	nick_light_t *nicklight;
	
	if(chanserv_args_check(nick, data, 3, "ACCESS #channel nick [+flag] (Note: nick '*' will show access list)"))
		return;
	
	// check if user is identified
	if(!(nick->umodes & UMODE_REGISTERED)) {
		irc_notice(nick->nick, "You are not identified");
		return;
	}
	
	// grabbing chan
	chan = string_index(data, 1);
	if(!(channel = list_search(global_lib.channels, chan))) {
		irc_notice(nick->nick, "Channel doesn't exists");
		free(chan);
		return;
	}
	
	free(chan);
	
	nicklight = list_search(channel->nicks, nick->nick);
	if(!(nicklight->modes & CMODE_USER_OWNER)) {
		irc_notice(nick->nick, "You are not currently channel owner");
		return;
	}
	
	user = string_index(data, 2);
	if(!strcmp(user, "*")) {
		chanserv_access_show(channel, nick);
		free(user);
		return;
	}
	
	if(!nickserv_user_exists(user)) {
		irc_notice(nick->nick, "This user is not registered");
		free(user);
		return;
	}
	
	// no flags
	if(!(flags = string_index(data, 3))) {
		printf("[+] chanserv/access: removing <%s> from access\n", user);
		
		sqlquery = sqlite3_mprintf(
			"DELETE FROM cs_access "
			"WHERE channel = '%q'  "
			"  AND gid = (SELECT gid FROM ns_nick WHERE nick = '%q')",
			channel->channel, user
		);
		
		if(db_sqlite_simple_query(sqlite_db, sqlquery)) {
			// if user connected, changing on the fly
			if((nicklight = list_search(channel->nicks, user))) {
				flags = chan_cumode_text(nicklight->modes);
				*flags = '-';
				
				printf("%s\n", flags);
				chan_cmode_single_edit(channel, flags, user);
			}
			
			irc_notice(nick->nick, "Access removed");
			
		} else irc_notice(nick->nick, "Cannot remove entry");
		
	} else {
		// check flags
		chan = NULL;
		
		for(testme = flags + 1; *testme; testme++) {			
			if(!strchr(__cumodes_list, *testme)) {
				chan = testme;
				break;
			}
		}
		
		printf("[+] chanserv/access: request flags <%s> to <%s>\n", flags, user);
		
		if(!chan && *flags == '+') {
			sqlquery = sqlite3_mprintf(
				"REPLACE INTO cs_access (channel, gid, flags) VALUES "
				"('%q', (SELECT gid FROM ns_nick WHERE nick = '%q'), '%q')",
				channel->channel, user, flags
			);
			
			if(db_sqlite_simple_query(sqlite_db, sqlquery)) {
				chan_cmode_single_edit(channel, flags, user);
				irc_notice(nick->nick, "Access updated");
				
			} else irc_notice(nick->nick, "Cannot update entry");
			
		} else irc_notice(nick->nick, "Invalid flags, should be like: +h");
	}
	
	free(flags);
	free(user);
}

void chanserv_info(nick_t *nick, char *data) {
	sqlite3_stmt *stmt;
	char *sqlquery, *chan, *rdate, *owner = NULL;
	char reply[1024];
	
	if(chanserv_args_check(nick, data, 2, "INFO #channel"))
		return;
	
	chan = string_index(data, 1);
	
	// channel access
	sqlquery = sqlite3_mprintf(
		"SELECT owner, strftime('%%d/%%m/%%Y %%H:%%M:%%S', rdate) "
		"FROM cs_channel WHERE UPPER(channel) = UPPER('%q')       ",
		chan
	);
	
	if((stmt = db_sqlite_select_query(sqlite_db, sqlquery))) {
		while(sqlite3_step(stmt) == SQLITE_ROW) {
			owner = (char *) sqlite3_column_text(stmt, 0);
			rdate = (char *) sqlite3_column_text(stmt, 1);
			
			zsnprintf(reply, "%s registered by %s, %s", chan, owner, rdate);
			irc_notice(nick->nick, reply);
		}
	
	} else fprintf(stderr, "[-] chanserv/info: cannot select\n");
	
	sqlite3_free(sqlquery);
	sqlite3_finalize(stmt);
	free(chan);
	
	if(!owner)
		irc_notice(nick->nick, "No match found");
}

void chanserv_help(nick_t *nick, char *data) {
	(void) data;
	unsigned int i;
	char reply[1024];
	
	irc_notice(nick->nick, "Available commands:");
	
	for(i = 0; i < sizeof(__chanserv) / sizeof(request_t); i++) {
		zsnprintf(reply, "    * %s", __chanserv[i].name);
		irc_notice(nick->nick, reply);
	}
}

void chanserv_query(nick_t *nick, char *data) {
	char *request;
	unsigned int i;
	
	request = strtolower(string_index(data, 0));
	
	for(i = 0; i < sizeof(__chanserv) / sizeof(request_t); i++) {
		if(!strcmp(__chanserv[i].name, request)) {
			__chanserv[i].ucb(nick, data);
			break;
		}
	}
	
	free(request);
}

void chanserv_ban_add(channel_t *channel, char *mask) {
	char *sqlquery;
	
	if(!(channel->cmodes & CMODE_REGISTERED))
		return;
	
	sqlquery = sqlite3_mprintf(
		"INSERT INTO cs_ban (channel, mask) VALUES ('%q', '%q')",
		channel->channel, mask
	);
	
	// creating group
	if(db_sqlite_simple_query(sqlite_db, sqlquery))
		printf("[+] channel %s ban %s: added\n", channel->channel, mask);
		
	else fprintf(stderr, "[-] channel %s ban %s: failed\n", channel->channel, mask);
	
	sqlite3_free(sqlquery);
}

void chanserv_ban_del(channel_t *channel, char *mask) {
	char *sqlquery;
	
	if(!(channel->cmodes & CMODE_REGISTERED))
		return;
	
	sqlquery = sqlite3_mprintf(
		"DELETE FROM cs_ban WHERE channel = '%q' AND mask = '%q'",
		channel->channel, mask
	);
	
	// creating group
	if(db_sqlite_simple_query(sqlite_db, sqlquery))
		printf("[+] channel %s ban %s: removed\n", channel->channel, mask);
		
	else fprintf(stderr, "[-] channel %s ban %s: failed\n", channel->channel, mask);
	
	sqlite3_free(sqlquery);
}
