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
#include "lib_admin.h"
#include "lib_module_operserv.h"

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
			
		raw_socket(data);
		
	} else printf("[-] admin: <%s> is not admin\n", nick->nick);
}
