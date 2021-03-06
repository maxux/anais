#ifndef __ANAIS_CORE_SSL_H
	#define __ANAIS_CORE_SSL_H

	#include <openssl/ssl.h>

	typedef struct {
		int sockfd;
		SSL *socket;
		SSL_CTX *sslContext;
		
	} ssl_socket_t;
	
	
	ssl_socket_t *init_socket_ssl(int sockfd, ssl_socket_t *ssl);
	int ssl_read(ssl_socket_t *ssl, char *data, int max);
	int ssl_write(ssl_socket_t *ssl, char *data);
	void ssl_close(ssl_socket_t *ssl);
	void ssl_error();
#endif
