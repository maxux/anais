#ifndef __ANAIS_LIB_NICK_H
	#define __ANAIS_LIB_NICK_H
	
	typedef enum umodes_t {
		UMODE_SERVER_ADMIN    = 1,        // +A
		UMODE_SERVICE_ADMIN   = 1 << 1,   // +a
		UMODE_BOT             = 1 << 2,   // +B
		UMODE_COADMIN         = 1 << 3,   // +C
		UMODE_CHPRIVMSG       = 1 << 4,   // +d
		UMODE_BADWORDS        = 1 << 5,   // +G
		UMODE_GL_LO_OPS       = 1 << 6,   // +g
		UMODE_HIDE_IRCOP      = 1 << 7,   // +H
		UMODE_AVAIL_HELP      = 1 << 8,   // +h
		UMODE_HIDE_OPER_IDLE  = 1 << 9,   // +I
		UMODE_INVISIBLE       = 1 << 10,  // +i
		UMODE_NET_ADMIN       = 1 << 11,  // +N
		UMODE_LOCAL_ADMIN     = 1 << 12,  // +O
		UMODE_GLOBAL_ADMIN    = 1 << 13,  // +o
		UMODE_HIDE_CHANS      = 1 << 14,  // +p
		UMODE_ULINES_KICK     = 1 << 15,  // +q
		UMODE_ALLOW_RMSG      = 1 << 16,  // +R
		UMODE_REGISTERED      = 1 << 17,  // +r
		UMODE_PDAEMONS        = 1 << 18,  // +S
		UMODE_SRV_NOTICE      = 1 << 19,  // +s
		UMODE_DISABLE_CTCP    = 1 << 20,  // +T
		UMODE_USE_VHOST       = 1 << 21,  // +t
		UMODE_WEBTV           = 1 << 22,  // +V
		UMODE_INFECT_DCC      = 1 << 23,  // +v
		UMODE_WHOIS_HIDDEN    = 1 << 24,  // +W
		UMODE_WALLOP          = 1 << 25,  // +w
		UMODE_HIDDEN_HOST     = 1 << 26,  // +x
		UMODE_USE_SSL         = 1 << 27,  // +z
		
	} umodes_t;

	typedef struct nick_t {
		char *nick;
		char *user;
		char *host;
		char *vhost;
		time_t timestamp;
		umodes_t umodes;
		const char *realname;
		char *away;
		int gid;
		char *class;
		char *fingerprint;
		
		list_t *channels;
		
	} nick_t;
	
	typedef enum allow_t {
		DISALLOW,
		ALLOWED,
		IDENTIFIED
		
	} allow_t;
	
	umodes_t nick_umode_parse(char *modes, umodes_t original);
	
	void nick_destruct(void *data);
	void nick_new(char *request);
	
	void nick_away(nick_t *nick, char *data);
	void nick_change(nick_t *nick, char *data);
	void nick_quit(nick_t *nick, char *data);
	void nick_part(nick_t *nick, char *data);
	void nick_kick(nick_t *nick, char *data);
	
	void nick_trace(server_t *server, char *data);	
	void nick_trace_init();
#endif
