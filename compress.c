#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "compress.h"

char_list encode_response(char_list resp, int dict_fd){

}

/* Add a string to the dictionnary */
void dict_add_string(dict *d, char_list new_str){
	char_list_print(new_str);
	//start at root
	dict_node* cur_node = &d->nodes[0];
	//TODO: progressively try out longer and longer substrings of the new string (A)
	//until we no longer find a match (B), at which point we start looking for a substring in the
	//new string which matches a later part of the same string chain that we split off
	//from in the first place (C)
	//
	//		A ——— B ————— C —————>			(existing string)
	//			   \_____/					(new string)
	//
	for (int i = 0; i < new_str.length; i++) {
		char_list_print(new_str);
		dict_print(*d);
		printf("current value: %c, looking for %c\n", cur_node->value, new_str.data[i]);
		//search which path has our next character
		int next_char_ind = node_has_next(*cur_node, new_str.data[i]);
		//if no path has our character, add it
		if(next_char_ind < 0){
			printf("didnt find it\n");
			int new_node_ind = dict_add_node(d, node_create(new_str.data[i]));
			node_add_next(cur_node, new_node_ind);
			cur_node = &d->nodes[new_node_ind];
		//if we find a path with our character, follow it
		}else{
			printf("found it\n");
			cur_node = &d->nodes[next_char_ind];
		}
	}
}
