#ifndef __ANAIS_LIB_CHAN_H
	#define __ANAIS_LIB_CHAN_H
	
	typedef enum cmodes_t {
		CMODE_ADMIN_ONLY      = 1,        // +A
		CMODE_NO_ANSI_COLOR   = 1 << 1,   // +c
		CMODE_NO_CTCP         = 1 << 2,   // +C
		CMODE_INVITE_REQUIRED = 1 << 3,   // +i
		CMODE_KNOCK_DENIED    = 1 << 4,   // +K
		CMODE_ONLY_REG_TALK   = 1 << 5,   // +M
		CMODE_MODERATED       = 1 << 6,   // +m
		CMODE_NO_NICK_CHANGE  = 1 << 7,   // +N
		CMODE_NO_MSG_OUTSIDE  = 1 << 8,   // +n
		CMODE_IRCOPS_ONLY     = 1 << 9,   // +O
		CMODE_PRIATE          = 1 << 10,  // +p
		CMODE_ONLY_ULINE_KICK = 1 << 11,  // +Q
		CMODE_REGISTERED      = 1 << 12,  // +r
		CMODE_ONLY_REG_JOIN   = 1 << 13,  // +R
		CMODE_STRIP_COLOR     = 1 << 14,  // +S
		CMODE_SECRET          = 1 << 15,  // +s
		CMODE_HALFOP_TOPIC    = 1 << 16,  // +t
		CMODE_NO_NOTICE       = 1 << 17,  // +T
		CMODE_AUDITORIUM      = 1 << 18,  // +u
		CMODE_NO_INVITE       = 1 << 19,  // +V
		CMODE_SSL_ONLY        = 1 << 20,  // +z
		CMODE_ALL_SSL         = 1 << 21,  // +Z
		
	} cmodes_t;
	
	typedef enum cumodes_t {
		CMODE_USER_ADMIN      = 1,       // +a
		CMODE_USER_HALFOP     = 1 << 1,  // +h
		CMODE_USER_OPERATOR   = 1 << 2,  // +o
		CMODE_USER_OWNER      = 1 << 3,  // +q
		CMODE_USER_VOICE      = 1 << 4,  // +v
		
	} cumodes_t;
	
	typedef struct nick_light_t {
		char *nick;
		cumodes_t modes;
		
	} nick_light_t;
	
	typedef struct topic_t {
		char *topic;
		time_t updated;
		char *author;
		
	} topic_t;

	typedef struct channel_t {
		char *channel;    // channel name
		cmodes_t cmodes;  // channel mode
		list_t *nicks;    // nick_lite_t
		
		struct topic_t topic;
		
	} channel_t;
	
	extern const char *__cumodes_list;
	
	nick_light_t *nick_light_new(char *nick);
	
	void chan_destruct(void *data);
	void chan_new(char *request);	
	
	char *chan_cmode_text(cmodes_t cmode);
	char *chan_cumode_text(cumodes_t cumode);
	
	cmodes_t chan_cmode_parse(channel_t *channel, char *modes, char *argv[]);
	void chan_cmode_single_edit(channel_t *channel, char *flags, char *user);
	
	void chan_save_flags(channel_t *channel);
	
	void chan_join(nick_t *nick, char *data);
	void chan_mode_server(server_t *server, char *data);
	void chan_mode_user(nick_t *nick, char *data);
	void chan_topic(char *data);
	void chan_topic_server(server_t *server, char *data);
	void chan_topic_user(nick_t *nick, char *data);
	
	void chan_dump(char *chan);
#endif
