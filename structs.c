/*
*
*	Structures used throughout program
*
*
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "structs.h"

/* dynamically sized char buffer */
char_list char_list_init(){
	char_list list;
	list.length = 0;
	list.mem_size = 2;
	list.data = malloc(list.mem_size * sizeof(char));
	return list;
}
void char_list_add(char_list *list, char * new, int new_len){
	if(list->mem_size - list->length < new_len){
		do{
			list->mem_size *= 2;
		}while(list->mem_size - list->length < new_len);
		list->data = realloc(list->data, list->mem_size);
	}
	strncpy(&list->data[list->length], new, new_len);
	list->length += new_len;
}
void char_list_print(char_list list){
	for (int i = 0; i < list.length; i++) {
		printf("%c", list.data[i]);
	}
	printf("\n");
}
char_list char_list_from_str(char * str){
	char_list list = char_list_init();
	char_list_add(&list, str, strlen(str));
	return list;
}

/* linked-list dictionnary for compressing strings */
dict dict_init(){
	dict d;
	d.length = 0;
	d.mem_size = 2;
	d.nodes = malloc(d.mem_size * sizeof(dict_node));
	return d;
}
/* add a node to dictionnary and update size values */
void dict_add_node(dict *d, dict_node n){
	d->length++;
	if(d->length > d->mem_size){
		d->mem_size *= 2;
		d->nodes = realloc(d->nodes, d->mem_size * sizeof(dict_node));
	}
	d->nodes[d->length-1] = n;
}

/* dictionnary nodes of which the dictionnary is made of */
dict_node make_node(char value){
	dict_node n;
	n.value = value;
	return n;
}
