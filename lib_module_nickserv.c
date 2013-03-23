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

#define FINGERPRINT_VHOST      "fingerprint.certified"
#define FINGERPRINT_CLASS      "sslcert"

#define irc_notice(...) irc_noticefrom(NICKSERV_NAME, __VA_ARGS__)

request_t __nickserv[] = {
	{.name = "register",    .ucb = nickserv_register},
	{.name = "identify",    .ucb = nickserv_identify},
	{.name = "password",    .ucb = nickserv_password},
	{.name = "email",       .ucb = nickserv_email},
	{.name = "group",       .ucb = nickserv_group},
	{.name = "sslcert",     .ucb = nickserv_sslcert},
	{.name = "vhost",       .ucb = nickserv_vhost},
	{.name = "info",        .ucb = nickserv_info},
	{.name = "help",        .ucb = nickserv_help},
};

void nickserv_registred(nick_t *nick) {
	char request[512];
	
	zsnprintf(request, "SVS2MODE %s +r", nick->nick);
	raw_socket(request);
	
	nick->umodes |= UMODE_REGISTERED;
}

char *nickserv_passwd(char *nick, char *password) {
	char hashme[512];
	
	zsnprintf(hashme, "%s%s", nick, password);
	return md5ascii(hashme);
}

char *nickserv_rootof(char *nick) {
	sqlite3_stmt *stmt;
	char *sqlquery;
	char *root = NULL;
	
	sqlquery = sqlite3_mprintf(
		"SELECT root FROM ns_group WHERE gid =          "
		"    (SELECT gid FROM ns_nick WHERE nick = '%q')",
		nick
	);
	
	if((stmt = db_sqlite_select_query(sqlite_db, sqlquery))) {
		while(sqlite3_step(stmt) == SQLITE_ROW)
			root = strdup((char *) sqlite3_column_text(stmt, 0));
	
	} else fprintf(stderr, "[-] nickserv/rootof: cannot select\n");
	
	sqlite3_free(sqlquery);
	sqlite3_finalize(stmt);
	
	return root;
}

int nickserv_args_check(nick_t *nick, char *data, int argc, char *syntax) {
	char answer[1024];
	
	if(words_count(data) < (unsigned int) argc) {
		zsnprintf(answer, "Missing arguments: %s", syntax);
		irc_notice(nick->nick, answer);
		return 1;
	}
	
	return 0;
}

allow_t nickserv_allow_protected(nick_t *nick) {
	sqlite3_stmt *stmt;
	char *sqlquery, *fingerprint, *username;
	allow_t allow = ALLOWED;
	
	sqlquery = sqlite3_mprintf(
		"SELECT fingerprint, username FROM ns_group WHERE gid = ("
		"   SELECT gid FROM ns_nick WHERE UPPER(nick) = UPPER('%q')"
		")",
		nick->nick
	);
	
	if((stmt = db_sqlite_select_query(sqlite_db, sqlquery))) {
		while(sqlite3_step(stmt) == SQLITE_ROW) {
			fingerprint = (char *) sqlite3_column_text(stmt, 0);
			username    = (char *) sqlite3_column_text(stmt, 1);
			
			if(fingerprint) {
				printf("[+] nickserv/allow: %s <> %s (%s)\n", 
				       username, nick->user, fingerprint);

				// deny
				if(nick->fingerprint && !strcmp(nick->fingerprint, fingerprint))
					allow = IDENTIFIED;
					
				else allow = DISALLOW;
				
			} else printf("[-] nickserv/allow: no fingerprint found\n");
		}
	
	} else fprintf(stderr, "[-] nickserv/allow: cannot select\n");
	
	sqlite3_free(sqlquery);
	sqlite3_finalize(stmt);
	
	return allow;
}

void nickserv_allow_check(nick_t *nick, allow_t allow) {
	char request[256], fake[64];
	
	if(allow == DISALLOW) {
		printf("[-] nickserv/allow_check: disallow\n");
		
		irc_notice(nick->nick, "This nick is protected by fingerprint");
		
		zsnprintf(fake, "anon%d", rand() / 1000);
		zsnprintf(request, "SVSNICK %s %s :%ld", nick->nick, fake, time(NULL));
		raw_socket(request);
		
	} else if(allow == IDENTIFIED) {
		printf("[-] nickserv/allow_check: identified\n");
		
		nickserv_registred(nick);
		nickserv_restore(nick);
		
	} else printf("[-] nickserv/allow_check: allowed\n");
}

int nickserv_user_exists(char *nickname) {
	sqlite3_stmt *stmt;
	char *sqlquery;
	int retcode = 0;
	
	sqlquery = sqlite3_mprintf(
		"SELECT COUNT(*) FROM ns_nick WHERE UPPER(nick) = UPPER('%q')",
		nickname
	);
	
	if((stmt = db_sqlite_select_query(sqlite_db, sqlquery))) {
		while(sqlite3_step(stmt) == SQLITE_ROW)
			retcode = sqlite3_column_int(stmt, 0);
	
	} else fprintf(stderr, "[-] nickserv/user_exists: cannot select\n");
	
	sqlite3_free(sqlquery);
	sqlite3_finalize(stmt);
	
	return retcode;
}

int nickserv_user_identify(char *nick, char *password) {
	sqlite3_stmt *stmt;
	char *sqlquery, *hashed;
	int gid = 0;
	
	// get email, hash password	
	hashed = nickserv_passwd(nick, password);
	
	sqlquery = sqlite3_mprintf(
		"SELECT g.gid FROM ns_group g, ns_nick n                        "
		"    WHERE n.gid = g.gid AND n.nick = '%q' AND g.password = '%s'",
		nick, hashed
	);
	
	if((stmt = db_sqlite_select_query(sqlite_db, sqlquery))) {
		while(sqlite3_step(stmt) == SQLITE_ROW)
			gid = sqlite3_column_int(stmt, 0);
	
	} else fprintf(stderr, "[-] nickserv/user_exists: cannot select\n");
	
	sqlite3_free(sqlquery);
	sqlite3_finalize(stmt);
	
	return gid;
}

void nickserv_register(nick_t *nick, char *data) {
	char *sqlquery, *password;
	char *hashed, *email;
	
	if(nickserv_args_check(nick, data, 3, "REGISTER password email"))
		return;
	
	// check if nick is not already registred
	if(nickserv_user_exists(nick->nick)) {
		irc_notice(nick->nick, "Nick already registered");
		return;
	}
	
	// get email, hash password
	password = string_index(data, 1);
	hashed   = nickserv_passwd(nick->nick, password);
	email    = string_index(data, 2);
	
	sqlquery = sqlite3_mprintf(
		"INSERT INTO ns_group (gid, email, password, vhost, root, username, fingerprint) "
		"VALUES (NULL, '%q', '%s', NULL, '%q', '%q', NULL)",
		email, hashed, nick->nick, nick->user
	);
	
	// creating group
	if(db_sqlite_simple_query(sqlite_db, sqlquery)) {
		sqlite3_free(sqlquery);
		
		// creating nick
		sqlquery = sqlite3_mprintf(
			"INSERT INTO ns_nick (nick, gid, rdate, lastdate) VALUES "
			"('%q', %ld, datetime('now', 'localtime'), datetime('now', 'localtime'))",
			nick->nick, sqlite3_last_insert_rowid(sqlite_db)
		);
		
		if(db_sqlite_simple_query(sqlite_db, sqlquery)) {
			irc_notice(nick->nick, "You are now registered");
			nickserv_registred(nick);
			nickserv_restore(nick);
			
		} else irc_notice(nick->nick, "Cannot register your nick");
		
	} else irc_notice(nick->nick, "Cannot register your group (required)");
	
	sqlite3_free(sqlquery);
	
	free(hashed);
	free(password);
}

void nickserv_restore(nick_t *nick) {
	sqlite3_stmt *stmt;
	char *sqlquery, *chan, *flags, *vhost;
	channel_t *channel;
	char temp[1024];
	
	// channel access
	sqlquery = sqlite3_mprintf(
		"SELECT channel, flags FROM cs_access WHERE UPPER(nick) = UPPER('%q')",
		nick->nick
	);
	
	if((stmt = db_sqlite_select_query(sqlite_db, sqlquery))) {
		while(sqlite3_step(stmt) == SQLITE_ROW) {
			chan  = (char *) sqlite3_column_text(stmt, 0);
			flags = (char *) sqlite3_column_text(stmt, 1);
			
			if((channel = list_search(global_lib.channels, chan)))
				chan_cmode_single_edit(channel, flags, nick->nick);
		}
	
	} else fprintf(stderr, "[-] nickserv/restore: cannot select\n");
	
	sqlite3_free(sqlquery);
	sqlite3_finalize(stmt);
	
	// vhost
	sqlquery = sqlite3_mprintf(
		"SELECT vhost FROM ns_group WHERE gid = ("
		"   SELECT gid FROM ns_nick WHERE nick = '%q'"
		") AND vhost IS NOT NULL",
		nick->nick
	);
	
	if((stmt = db_sqlite_select_query(sqlite_db, sqlquery))) {
		while(sqlite3_step(stmt) == SQLITE_ROW) {
			vhost = (char *) sqlite3_column_text(stmt, 0);
			
			zsnprintf(temp, "CHGHOST %s %s", nick->nick, vhost);
			raw_socket(temp);
		}
	
	} else fprintf(stderr, "[-] nickserv/restore: cannot select\n");
	
	sqlite3_free(sqlquery);
	sqlite3_finalize(stmt);
}

void nickserv_identify(nick_t *nick, char *data) {
	char *password, *root;
	char *sqlquery;
	(void) data;
	
	if(nickserv_args_check(nick, data, 2, "IDENTIFY password"))
		return;
	
	if(nick->umodes & UMODE_REGISTERED) {
		irc_notice(nick->nick, "You are already identified");
		return;
	}
	
	password = string_index(data, 1);
	root     = nickserv_rootof(nick->nick);
	
	if((nick->gid = nickserv_user_identify(root, password))) {
		irc_notice(nick->nick, "You are now identified, welcome back :)");
		nickserv_registred(nick);
		nickserv_restore(nick);
		
		sqlquery = sqlite3_mprintf(
			"UPDATE ns_nick SET lastdate = datetime('now', 'localtime') "
			" WHERE nick = '%q'",
			nick->nick
		);
		
		if(!db_sqlite_simple_query(sqlite_db, sqlquery))
			fprintf(stderr, "[-] nickserv/identify: cannot update lastdate\n");
		
	} else irc_notice(nick->nick, "Wrong password");
	
	free(password);
}

void nickserv_password(nick_t *nick, char *data) {
	char *pass, *hashed, *sqlquery, *root;
	
	if(nickserv_args_check(nick, data, 3, "PASSWORD current-password new-password"))
		return;
	
	if(!(nick->umodes & UMODE_REGISTERED)) {
		irc_notice(nick->nick, "You are not identified");
		return;
	}
	
	pass = string_index(data, 1);
	if(nickserv_user_identify(nick->nick, pass)) {
		free(pass);
		
		pass   = string_index(data, 2);
		root   = nickserv_rootof(nick->nick);
		hashed = nickserv_passwd(root, pass);
		
		sqlquery = sqlite3_mprintf(
			"UPDATE ns_group SET password = '%s' WHERE gid = "
			"   (SELECT gid FROM ns_nick WHERE nick = '%q')  ",
			hashed, nick->nick
		);
		
		// updating password
		if(db_sqlite_simple_query(sqlite_db, sqlquery)) {
			irc_notice(nick->nick, "Password updated");
			
		} else irc_notice(nick->nick, "Cannot update your password");
		
		sqlite3_free(sqlquery);
		
	} else irc_notice(nick->nick, "Wrong password");
	
	free(pass);
}

void nickserv_email(nick_t *nick, char *data) {
	char *pass, *sqlquery;
	char *email = NULL;
	
	if(nickserv_args_check(nick, data, 3, "EMAIL current-password new-email"))
		return;
	
	if(!(nick->umodes & UMODE_REGISTERED)) {
		irc_notice(nick->nick, "You are not identified");
		return;
	}
	
	pass = string_index(data, 1);
	if(nickserv_user_identify(nick->nick, pass)) {
		email = string_index(data, 2);
		
		sqlquery = sqlite3_mprintf(
			"UPDATE ns_group SET email = '%q' WHERE gid = "
			"   (SELECT gid FROM ns_nick WHERE nick = '%q')  ",
			email, nick->nick
		);
		
		// updating password
		if(db_sqlite_simple_query(sqlite_db, sqlquery)) {
			irc_notice(nick->nick, "Email updated");
			
		} else irc_notice(nick->nick, "Cannot update your email");
		
		sqlite3_free(sqlquery);
		
	} else irc_notice(nick->nick, "Wrong password");
	
	free(pass);
	free(email);
}

void nickserv_group(nick_t *nick, char *data) {
	char *pass, *sqlquery, *mainnick;
	
	if(nickserv_args_check(nick, data, 3, "GROUP main-password main-nick"))
		return;
	
	pass = string_index(data, 1);
	mainnick = string_index(data, 2);
	
	if(nickserv_user_identify(mainnick, pass)) {
		sqlquery = sqlite3_mprintf(
			"INSERT INTO ns_nick (nick, gid) "
			"   SELECT '%q', gid FROM ns_nick WHERE nick = '%q'",
			nick->nick, mainnick
		);
		
		// updating password
		if(db_sqlite_simple_query(sqlite_db, sqlquery)) {
			irc_notice(nick->nick, "Nick groupped");
			nickserv_registred(nick);
			
		} else irc_notice(nick->nick, "Cannot update group");
		
		sqlite3_free(sqlquery);
		
	} else irc_notice(nick->nick, "Wrong password");
	
	free(pass);
	free(mainnick);
}

void nickserv_vhost(nick_t *nick, char *data) {
	char *vhost, temp[256], *sqlquery;
	
	if(nickserv_args_check(nick, data, 1, "VHOST vhost"))
		return;
	
	if(!(nick->umodes & UMODE_REGISTERED)) {
		irc_notice(nick->nick, "You are not identified");
		return;
	}
	
	if((vhost = string_index(data, 1))) {
		if(!strcmp(vhost, FINGERPRINT_VHOST)) {
			irc_notice(nick->nick, "vhost denied");
			free(vhost);
			return;
		}
		
		sqlquery = sqlite3_mprintf(
			"UPDATE ns_group SET vhost = '%q' WHERE gid = ("
			"   SELECT gid FROM ns_nick WHERE nick = '%q'"
			")",
			vhost, nick->nick
		);
			
		// updating password
		if(db_sqlite_simple_query(sqlite_db, sqlquery)) {
			zsnprintf(temp, "CHGHOST %s %s", nick->nick, vhost);
			raw_socket(temp);
			
			nick->umodes |= UMODE_USE_VHOST;
			
			irc_notice(nick->nick, "vhost saved and set");
				
		} else irc_notice(nick->nick, "Cannot update vhost");
	} else {
		sqlquery = sqlite3_mprintf(
			"UPDATE ns_group SET vhost = NULL WHERE gid = ("
			"   SELECT gid FROM ns_nick WHERE nick = '%q'"
			")",
			nick->nick
		);
			
		// updating password
		if(db_sqlite_simple_query(sqlite_db, sqlquery)) {			
			irc_notice(nick->nick, "vhost removed");
			zsnprintf(temp, "SVSMODE %s -xt", nick->nick);
			raw_socket(temp);
			
			zsnprintf(temp, "SVSMODE %s +x", nick->nick);
			raw_socket(temp);
			
			nick->umodes &= ~UMODE_USE_VHOST;
				
		} else irc_notice(nick->nick, "Cannot update vhost");
	}
	
	free(vhost);
}

void nickserv_info(nick_t *nick, char *data) {
	sqlite3_stmt *stmt;
	char *sqlquery, *root, *rdate, *lastdate, *vhost, *who;
	char *username, *fingerprint;
	int gid = -1;
	char reply[1024];
	
	if(nickserv_args_check(nick, data, 2, "INFO nick"))
		return;
	
	who = string_index(data, 1);
	
	// channel access
	sqlquery = sqlite3_mprintf(
		"SELECT g.gid, g.vhost, g.root, g.username, g.fingerprint,           "
		"       strftime('%%d/%%m/%%Y %%H:%%M:%%S', n.rdate, 'localtime'),   "
		"       strftime('%%d/%%m/%%Y %%H:%%M:%%S', n.lastdate, 'localtime') "
		"  FROM ns_group g, ns_nick n                                        "
		" WHERE g.gid = n.gid AND UPPER(n.nick) = UPPER('%q')                ",
		who
	);
	
	if((stmt = db_sqlite_select_query(sqlite_db, sqlquery))) {
		while(sqlite3_step(stmt) == SQLITE_ROW) {
			gid   = (int) sqlite3_column_int(stmt, 0);
			vhost = (char *) sqlite3_column_text(stmt, 1);
			root  = (char *) sqlite3_column_text(stmt, 2);
			
			username    = (char *) sqlite3_column_text(stmt, 3);
			fingerprint = (char *) sqlite3_column_text(stmt, 4);
			
			rdate = (char *) sqlite3_column_text(stmt, 5);
			lastdate = (char *) sqlite3_column_text(stmt, 6);
			
			zsnprintf(reply, "%s, username: %s, gid: %d (root nick: %s)", 
			                  who, username, gid, root);

			irc_notice(nick->nick, reply);
			
			if(vhost) {
				zsnprintf(reply, "virtual host      : %s", vhost);
				irc_notice(nick->nick, reply);
			}
			
			zsnprintf(reply, "registered date   : %s", rdate);
			irc_notice(nick->nick, reply);
			
			zsnprintf(reply, "last identify date: %s", lastdate);
			irc_notice(nick->nick, reply);
			
			if(fingerprint) {
				zsnprintf(reply, "ssl fingerprint   : %s", fingerprint);
				irc_notice(nick->nick, reply);
			}
		}
	
	} else fprintf(stderr, "[-] nickserv/info: cannot select\n");
	
	sqlite3_free(sqlquery);
	sqlite3_finalize(stmt);
	free(who);
	
	if(gid == -1)
		irc_notice(nick->nick, "No match found");
}

void nickserv_sslcert(nick_t *nick, char *data) {
	(void) data;
	char *sqlquery, *fingerprint;
	
	if(nickserv_args_check(nick, data, 2, "SSLCERT your-fingerprint (SHA-256, format AA:BB:CC)"))
		return;
	
	if(!nick->class || strcmp(nick->class, FINGERPRINT_CLASS)) {
		irc_notice(nick->nick, "You must use a certificate for that");
		return;
	}
	
	if(!nickserv_user_exists(nick->nick)) {
		irc_notice(nick->nick, "You must register your pseudo before");
		return;
	}
	
	fingerprint = string_index(data, 1);
	
	sqlquery = sqlite3_mprintf(
		"UPDATE ns_group SET fingerprint = '%q' WHERE gid = ("
		"   SELECT gid FROM ns_nick WHERE nick = '%q'"
		") AND username = '%q'",
		fingerprint, nick->nick, nick->user
	);
		
	// updating password
	if(db_sqlite_simple_query(sqlite_db, sqlquery)) {
		irc_notice(nick->nick, "Fingerprint saved. Your nick is protected "
		                       "and you will be auto-ident naow");
		
		nick->fingerprint = strdup(fingerprint);
		nickserv_fingerprint(nick);
		
	} else irc_notice(nick->nick, "Cannot save your fingerprint");
	
	free(fingerprint);
}

void nickserv_help(nick_t *nick, char *data) {
	(void) data;
	unsigned int i;
	char reply[1024];
	
	irc_notice(nick->nick, "Available commands:");
	
	for(i = 0; i < sizeof(__nickserv) / sizeof(request_t); i++) {
		zsnprintf(reply, "    * %s", __nickserv[i].name);
		irc_notice(nick->nick, reply);
	}
}

void nickserv_query(nick_t *nick, char *data) {
	char *request;
	unsigned int i;
	
	request = strtolower(string_index(data, 0));
	
	for(i = 0; i < sizeof(__nickserv) / sizeof(request_t); i++) {
		if(!strcmp(__nickserv[i].name, request)) {
			__nickserv[i].ucb(nick, data);
			break;
		}
	}
	
	free(request);
}

void nickserv_fingerprint(nick_t *nick) {
	sqlite3_stmt *stmt;
	char *sqlquery, *fp;
	char request[1024];
	allow_t allow;
	
	sqlquery = sqlite3_mprintf(
		"SELECT fingerprint FROM ns_group WHERE username = '%q' "
		"   AND fingerprint IS NOT NULL",
		nick->user
	);
	
	if((stmt = db_sqlite_select_query(sqlite_db, sqlquery))) {
		while(sqlite3_step(stmt) == SQLITE_ROW) {
			fp = (char *) sqlite3_column_text(stmt, 0);
			
			// saving fingerprint
			nick->fingerprint = strdup(fp);
			
			zsnprintf(request, "SWHOIS %s :SHA-256 fingerprint: %s", nick->nick, fp);
			raw_socket(request);
			
			zsnprintf(request, "CHGHOST %s %s", nick->nick, FINGERPRINT_VHOST);
			raw_socket(request);
		}
	
	} else fprintf(stderr, "[-] nickserv/fingerprint: cannot select\n");
	
	sqlite3_free(sqlquery);
	sqlite3_finalize(stmt);
	
	// auto-ident
	allow = nickserv_allow_protected(nick);
	nickserv_allow_check(nick, allow);
}
