/*
*
*	Structures used throughout program
*
*
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
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
	d.length = 1;
	d.nodes_size = 2;
	d.nodes = malloc(d.nodes_size * sizeof(dict_node));
	//add root node
	d.nodes[0] = node_create(0);
	return d;
}
/* add a node to dictionnary and update size values */
int dict_add_node(dict *d, dict_node n){
	//reallocate memory if needed
	d->length++;
	if(d->length > d->nodes_size){
		d->nodes_size *= 2;
		d->nodes = (dict_node*) realloc(d->nodes, d->nodes_size * sizeof(dict_node));
	}
	//add the node
	n.index = d->length-1;
	d->nodes[d->length-1] = n;
	return d->length-1;
}

/* show a simple printout of a dictionnary, in tree form */
void dict_print(dict d){
	for (int i = 0; i < d.length; i++) {
		unsigned int addr = (uintptr_t) &d.nodes[i];
		printf("(%c)\taddr: 0x%x,\tnext (%d): ", d.nodes[i].value, addr, d.nodes[i].branches_size);
		for (int j = 0; j < d.nodes[i].branches_size; j++) {
			printf("0x%x ", d.nodes[i].branches[j]);
		}
		printf("\n");
	}
	//TODO: do a BFS and print appropriate lines with | and /
}

/* dictionnary nodes of which the dictionnary is made of */
dict_node node_create(char value){
	dict_node n;
	n.value = value;
	n.branches_size = 0;
	n.branches = NULL;
	n.index = 0;
	return n;
}

/* add another link downstream to node */
void node_add_next(dict_node *n, int next){
	//add another branch
	n->branches_size++;
	n->branches = (int*) realloc(n->branches, n->branches_size * sizeof(int));
	n->branches[n->branches_size-1] = next;
}

/* gets index of next node with given character value (NULL if not found) */
int node_has_next(dict_node *n, char c){
	for (int i = 0; i < n->branches_size; i++) {
		dict_node *next = n - n->index + n->branches[i];		//pointer arithmetic is weird
		if(next->value == c){
			return n->branches[i];
		}
	}
	return -1;
}
