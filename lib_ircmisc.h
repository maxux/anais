#ifndef __ANAIS_LIB_IRCMISC_H
	#define __ANAIS_LIB_IRCMISC_H

	char *ltrim(char *str);
	char *rtrim(char *str);
	char *crlftrim(char *str);
	
	char *space_encode(char *str);
	void intswap(int *a, int *b);
	
	char *irc_mstrncpy(char *src, size_t len);
	char *skip_header(char *data);
	
	char *md5ascii(char *source);
	
	char *string_index(char *str, unsigned int index);
	
	char *strtolower(char *str);
	size_t words_count(char *str);
#endif
