#ifndef __ANAIS_LIB_MODULE_CHANSERV_H
	#define __ANAIS_LIB_MODULE_CHANSERV_H

	void chanserv_registred(char *chan);
	int chanserv_chan_exists(char *chan);
	
	void chanserv_register(nick_t *nick, char *data);
	void chanserv_access(nick_t *nick, char *data);
	void chanserv_info(nick_t *nick, char *data);
	void chanserv_help(nick_t *nick, char *data);
	
	void chanserv_load(channel_t *channel);
	void chanserv_query(nick_t *nick, char *data);
	
	void chanserv_ban_add(channel_t *channel, char *mask);
	void chanserv_ban_del(channel_t *channel, char *mask);
#endif
