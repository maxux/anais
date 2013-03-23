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
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lib_list.h"

list_t * list_init(void (*destruct)(void *)) {
	list_t *list;
	
	if(!(list = (list_t*) malloc(sizeof(list_t))))
		return NULL;
	
	list->destruct  = destruct;
	list->nodes     = NULL;
	list->length    = 0;
	
	return list;
}

void list_free(list_t *list) {
	list_node_t *node = list->nodes, *temp;
	
	/* Free resident data */
	while(node) {
		if(list->destruct)
			list->destruct(node->data);
			
		free(node->name);
		
		temp = node;
		node = node->next;
		
		free(temp);
	}
	
	free(list);
}

void * list_append(list_t *list, char *name, void *data) {
	list_node_t *node;
	
	if(!(node = (list_node_t*) malloc(sizeof(list_node_t))))
		return NULL;
	
	node->name = strdup(name);
	node->data = data;
	
	node->next  = list->nodes;	
	list->nodes = node;
	
	list->length++;
	
	return data;
}

void * list_search(list_t *list, char *name) {
	list_node_t *node = list->nodes;
	
	while(node && strcmp(node->name, name))
		node = node->next;
	
	return (node) ? node->data : NULL;
}

int list_remove(list_t *list, char *name) {
	list_node_t *node = list->nodes;
	list_node_t *prev = list->nodes;
	
	while(node && strcmp(node->name, name)) {
		prev = node;
		node = node->next;
	}
	
	if(!node)
		return 1;
	
	if(node == list->nodes)
		list->nodes = node->next;
		
	else prev->next = node->next;
	
	list->length--;
	
	/* Call user destructor */
	if(list->destruct)
		list->destruct(node->data);
	
	free(node->name);
	free(node);
	
	return 0;
}

// remove without calling destructor
int list_remove_only(list_t *list, char *name) {
	list_node_t *node = list->nodes;
	list_node_t *prev = list->nodes;
	
	while(node && strcmp(node->name, name)) {
		prev = node;
		node = node->next;
	}
	
	if(!node)
		return 1;
	
	if(node == list->nodes)
		list->nodes = node->next;
		
	else prev->next = node->next;
	
	list->length--;
	
	free(node->name);
	free(node);
	
	return 0;
}

void list_dump(list_t *list) {
	list_node_t *node = list->nodes;
	
	while(node) {
		printf("[ ] list (%p): %s/%p\n", (void*) list, node->name, node->data);
		node = node->next;
	}
}

void list_iterate(list_t *list, list_iter_callback callback, void *u1, void *u2) {
	list_node_t *node = list->nodes;
	
	while(node) {
		callback(node->data, u1, u2);
		node = node->next;
	}
}
