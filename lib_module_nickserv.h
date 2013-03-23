#ifndef __ANAIS_LIB_MODULE_NICKSERV_H
	#define __ANAIS_LIB_MODULE_NICKSERV_H

	void nickserv_register(nick_t *nick, char *data);
	void nickserv_identify(nick_t *nick, char *data);
	void nickserv_password(nick_t *nick, char *data);
	void nickserv_email(nick_t *nick, char *data);
	void nickserv_group(nick_t *nick, char *data);
	void nickserv_vhost(nick_t *nick, char *data);
	void nickserv_sslcert(nick_t *nick, char *data);
	void nickserv_info(nick_t *nick, char *data);
	void nickserv_help(nick_t *nick, char *data);
	void nickserv_query(nick_t *nick, char *data);
	
	// restore permissions, ...
	void nickserv_restore(nick_t *nick);
	
	// mask user as registered
	void nickserv_registred(nick_t *nick);
	
	// check if user exists on database
	int nickserv_user_exists(char *nickname);
	
	// check if fingerprint/username is allowed
	allow_t nickserv_allow_protected(nick_t *nick);
	void nickserv_allow_check(nick_t *nick, allow_t allow);
	
	void nickserv_fingerprint(nick_t *nick);
#endif
