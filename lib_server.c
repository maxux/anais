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
#include "lib_list.h"
#include "lib_core.h"
#include "lib_server.h"
#include "lib_ircmisc.h"

void server_destruct(void *data) {
	server_t *server = (server_t *) data;
	
	free(server->server);
	free(server);
}

void server_new(char *name) {
	server_t *server;
	
	if(!(server = (server_t *) calloc(1, sizeof(server_t))))
		diep("[-] calloc");
	
	server->server = strdup(name);
	
	list_append(global_lib.servers, (char *) name, server);
	
	// list_dump(global_lib.servers);
}

void server_server(char *data) {
	char *server;
	
	server = string_index(data + 1, 0);
	printf("[+] server: new server <%s>\n", server);
	server_new(server);
	free(server);	
}

void server_quit(server_t *server, char *data) {
	(void) data;
	
	// FIXME
	printf("[+] squit: removing %s\n", server->server);
	// list_remove(global_lib.nicks, nick->nick);
}
