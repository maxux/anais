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
#include <ctype.h>
#include <dlfcn.h>
#include <signal.h>
#include <setjmp.h>
#include <execinfo.h>
#include <sys/mman.h>
#include <pthread.h>
#include "services.h"
#include "core_init.h"
#include "lib_core.h"

ssl_socket_t *ssl;
global_core_t global_core;

void diep(char *str) {
	perror(str);
	exit(EXIT_FAILURE);
}

int signal_intercept(int signal, void (*function)(int)) {
	struct sigaction sig;
	int ret;
	
	/* Building empty signal set */
	sigemptyset(&sig.sa_mask);
	
	/* Building Signal */
	if(signal == SIGCHLD) {
		sig.sa_handler = SIG_IGN;
		sig.sa_flags   = SA_NOCLDWAIT;
	
	} else {
		sig.sa_handler = function;
		sig.sa_flags   = 0;
	}
	
	/* Installing Signal */
	if((ret = sigaction(signal, &sig, NULL)) == -1)
		perror("sigaction");
	
	return ret;
}

void sighandler(int signal) {
	void * buffer[1024];
	int calls;
	
	switch(signal) {
		case SIGSEGV:
			fprintf(stderr, "[-] --- Segmentation fault ---\n");
			calls = backtrace(buffer, sizeof(buffer) / sizeof(void *));
			backtrace_symbols_fd(buffer, calls, 1);
			exit(EXIT_FAILURE);
		break;
	}
}

/*
 * IRC Protocol
 */
char *skip_server(char *data) {
	int i, j;
	
	j = strlen(data);
	for(i = 0; i < j; i++)
		if(*(data+i) == ' ')
			return data + i + 1;
	
	return NULL;
}

int init_socket(char *server, int port) {
	int fd = -1, connresult;
	struct sockaddr_in server_addr;
	struct hostent *he;
	struct timeval tv = {tv.tv_sec = SOCKET_TIMEOUT, tv.tv_usec = 0};
	
	/* Resolving name */
	if((he = gethostbyname(server)) == NULL)
		perror("[-] socket: gethostbyname");
	
	bcopy(he->h_addr, &server_addr.sin_addr, he->h_length);

	server_addr.sin_family = AF_INET; 
	server_addr.sin_port = htons(port);

	/* Creating Socket */
	if((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("[-] socket: socket");
		return -1;
	}

	/* Init Connection */
	if((connresult = connect(fd, (struct sockaddr *) &server_addr, sizeof(server_addr))) < 0) {
		perror("[-] socket: connect");
		close(fd);
		return -1;
	}
	
	if(setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (char *) &tv, sizeof(tv)))
		diep("[-] setsockopt: SO_RCVTIMEO");
	
	return fd;
}

void raw_socket(char *message) {
	char *sending = (char *) malloc(sizeof(char *) * strlen(message) + 3);
	
	printf("[+] raw: << %s\n", message);
	
	strcpy(sending, message);
	strcat(sending, "\r\n");
	
	if(ssl_write(ssl, sending) == -1)
		perror("[-] IRC: send");
	
	free(sending);
}

int read_socket(ssl_socket_t *ssl, char *data, char *next) {
	char buff[MAXBUFF];
	int rlen, i, tlen;
	char *temp = NULL;
	
	buff[0] = '\0';		// Be sure that buff is empty
	data[0] = '\0';		// Be sure that data is empty	
	
	while(1)  {
		free(temp);
		temp = (char *) malloc(sizeof(char *) * (strlen(next) + MAXBUFF));
		
		tlen = sprintf(temp, "%s%s", next, buff);
		
		for(i = 0; i < tlen; i++) {			// Checking if next (+buff), there is not \r\n
			if(temp[i] == '\r' && temp[i+1] == '\n') {
				strncpy(data, temp, i);		// Saving Current Data
				data[i] = '\0';
				
				if(temp[i+2] != '\0' && temp[i+1] != '\0' && temp[i] != '\0') {		// If the paquet is not finished, saving the rest
					strncpy(next, temp+i+2, tlen - i);
					
				} else next[0] = '\0';
				
				free(temp);
				return 0;
			}
		}
		
		do {
			if((rlen = ssl_read(ssl, buff, MAXBUFF)) >= 0) {
				if(rlen == 0) {
					ssl_error();
					printf("[ ] core: Warning: nothing read from socket\n");
					exit(EXIT_FAILURE);
				}
					
				buff[rlen] = '\0';
				
			} else if(errno != EAGAIN)
				ssl_error();
			
		} while(errno == EAGAIN);
	}
	
	return 0;
}

int main(void) {
	char *data = (char *) calloc(sizeof(char), (2 * MAXBUFF));
	char *next = (char *) calloc(sizeof(char), (2 * MAXBUFF));
	char *request;
	
	printf("[+] core: loading...\n");
	
	/* Init random */
	srand(time(NULL));
	
	/* Initializing global variables */
	// settings variable
	global_core.startup_time = time(NULL);
	
	/* signals */
	signal_intercept(SIGSEGV, sighandler);
	signal_intercept(SIGCHLD, sighandler);
	
	main_construct();
	
	printf("[+] core: connecting...\n");
	
	// connect the basic socket
	if((global_core.sockfd = init_socket(IRC_REMOTE_SERVER, IRC_REMOTE_PORT)) < 0) {
		fprintf(stderr, "[-] core: cannot create socket\n");
		exit(EXIT_FAILURE);
	}
	
	// enable ssl layer
	if(IRC_REMOTE_SSL) {
		printf("[+] core: init ssl layer\n");
		if(!(ssl = init_socket_ssl(global_core.sockfd, &global_core.ssl))) {
			fprintf(stderr, "[-] core: cannot link ssl to socket\n");
			exit(EXIT_FAILURE);
		}
		
	} else printf("[-] core: ssl layer disabled\n");
	
	printf("[+] core: connected\n");
	
	raw_socket("PROTOCTL NICKv2 VHP");
	raw_socket("PASS " PRIVATE_PASSWORD);
	raw_socket("SERVER " IRC_LOCAL_SERVER " 1 :" IRC_LOCAL_NAME);
	
	while(1) {		
		read_socket(ssl, data, next);
		printf("[ ] raw: >> %s\n", data);
		
		if((request = skip_server(data)) == NULL) {
			printf("[-] irc: something wrong with protocol...\n");
			continue;
		}
		
		main_core(data, request);
	}
	
	// should never happend
	free(data);
	free(next);
	
	return 0;
}
