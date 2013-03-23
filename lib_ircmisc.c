/* anais - irc services
 * Author: Daniel Maxime (root@maxux.net)
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
#include <ctype.h>
#include <openssl/md5.h>
#include "core_init.h"
#include "lib_database.h"
#include "lib_core.h"
#include "lib_ircmisc.h"

char *ltrim(char *str) {
	while(isspace(*str))
		str++;
		
	return str;
}

char *rtrim(char *str) {
	char *back = str + strlen(str);
	
	while(isspace(*--back));
		*(back + 1) = '\0';
		
	return str;
}

char *crlftrim(char *str) {
	char *keep = str;
	
	while(*str) {
		if(*str == '\r' || *str == '\n')
			*str = ' ';
		
		str++;
	}
	
	return keep;
}

void intswap(int *a, int *b) {
	int c = *b;
	
	*b = *a;
	*a = c;
}

char *irc_mstrncpy(char *src, size_t len) {
	char *str;
	
	if(!(str = (char *) malloc(sizeof(char) * len + 1)))
		return NULL;
	
	strncpy(str, src, len);
	str[len] = '\0';
	
	return str;
}

char *string_index(char *str, unsigned int index) {
	unsigned int i;
	char *match;
	
	for(i = 0; i < index; i++) {
		if(!(match = strchr(str, ' ')))
			return NULL;
		
		while(isspace(*match))
			match++;
		
		str = match;
	}
	
	if(strlen(str) > 0) {
		if((match = strchr(str, ' '))) {
			return strndup(str, match - str);
			
		} else return strndup(str, strlen(str));
		
	} else return NULL;
}

char *skip_header(char *data) {
	char *match;
	
	if((match = strchr(data + 1, ':')))
		return match + 1;
		
	else return NULL;
}

char *md5ascii(char *source) {
	unsigned char result[MD5_DIGEST_LENGTH];
	const unsigned char *str;
	char *output, tmp[3];
	int i;
	
	printf("[+] ircmisc/md5: hashing <%s>\n", source);
	
	str = (unsigned char *) source;
	
	MD5(str, strlen(source), result);
	
	output = (char *) calloc(sizeof(char), (MD5_DIGEST_LENGTH * 2) + 1);
	
	for(i = 0; i < MD5_DIGEST_LENGTH; i++) {
		sprintf(tmp, "%02x", result[i]);
		strcat(output, tmp);
	}
	
	return output;
}

char *strtolower(char *str) {
	char *str2 = str;
	
	while(*str2)
		*str2++ = tolower(*str2);
	
	return str;
}

size_t words_count(char *str) {
	size_t words = 0;
	
	// empty string
	if(!*str)
		return 0;
	
	while(*str) {
		if(isspace(*str) && !isspace(*(str + 1)))
			words++;
		
		str++;
	}
	
	// last char was not a space
	if(!isspace(*(str - 1)))
		words++;
	
	return words;
}
