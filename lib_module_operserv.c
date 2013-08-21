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
#include "lib_admin.h"
#include "lib_module_operserv.h"
#include "lib_module_chanserv.h"

void operserv_restore_channel_callback(void *_channel, void *dummy1, void *dummy2) {
	(void) dummy1;
	(void) dummy2;
	channel_t *channel = (channel_t *) _channel;
	
	chanserv_load(channel);
}

void operserv_restore() {
	list_iterate(global_lib.channels, operserv_restore_channel_callback, NULL, NULL);
}

void operserv_reload(char *option) {
	channel_t *channel;
	
	if(!(channel = list_search(global_lib.channels, option))) {
		fprintf(stderr, "[-] reload: channel <%s> not found\n", option);
		return;
	}
	
	chanserv_load(channel);
}

void operserv_query(nick_t *nick, char *data) {
	char query[4096];
	
	if(admin_check(nick->nick, nick->host)) {
		printf("[+] admin: <%s> request: <%s>\n", nick->nick, data);
		
		// skipping stat commands
		if(!strncasecmp(data, "stat", 4))
			return;
		
		if(!strncasecmp(data, "global ", 7)) {
			zsnprintf(query, ":OperServ NOTICE $* :[%s] %s", nick->nick, data + 7);
			raw_socket(query);
			return;
		}
		
		if(!strncasecmp(data, "restore ", 7)) {
			/* restoring topic */
			operserv_restore();			
			return;
		}
		
		if(!strncasecmp(data, "reload ", 7)) {
			/* restoring topic */
			operserv_reload(data + 7);
			return;
		}
			
		raw_socket(data);
		
	} else printf("[-] admin: <%s> is not admin\n", nick->nick);
}
