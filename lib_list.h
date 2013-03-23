#ifndef __ANAIS_LIB_LIST_H
	#define __ANAIS_LIB_LIST_H
	
	typedef struct list_node_t {
		char *name;
		void *data;
		struct list_node_t *next;
		
	} list_node_t;

	typedef struct list_t {
		void (*destruct)(void *data);
		struct list_node_t *nodes;
		unsigned int length;
		
	} list_t;
	
	typedef void (*list_iter_callback)(void *, void *, void *);
	
	list_t *list_init(void (*destruct)(void *));
	
	void *list_append(list_t *list, char *name, void *data);
	void *list_search(list_t *list, char *name);
	
	int list_remove(list_t *list, char *name);
	int list_remove_only(list_t *list, char *name);
	
	void list_free(list_t *list);
	
	void list_iterate(list_t *list, list_iter_callback callback, void *u1, void *u2);
	void list_dump(list_t *list);
#endif
