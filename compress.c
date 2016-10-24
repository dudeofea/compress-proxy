#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "compress.h"

char_list encode_response(char_list resp, int dict_fd){

}

/* Add a string to the dictionnary */
void dict_add_string(dict *d, char_list new_str){
	//start at root
	dict_node* cur_node = &d->nodes[0];
	dict_node* main_branch = NULL;		//for when we're on a new branch
	//progressively try out longer and longer substrings of the new string (A)
	//until we no longer find a match (B), at which point we start looking for a substring in the
	//new string which matches a later part of the same string chain that we split off
	//from in the first place (C)
	//
	//		A ——— B ————— C —————>			(existing string)
	//			   \_____/					(new string)
	//
	for (int i = 0; i < new_str.length; i++) {
		//dict_print(*d);
		if(main_branch != NULL){
			main_branch = &d->nodes[main_branch->index];	//re-calc from possible resize
			//look for a character to reconnect us to the main branch again
			int merge_char_ind = node_has_next(main_branch, new_str.data[i]);
			if(merge_char_ind >= 0){
				//set as next and move forward
				node_add_next(cur_node, merge_char_ind);
				cur_node = &d->nodes[merge_char_ind];
				main_branch = NULL;
				continue;
			}
		}
		//search which path has our next character
		int next_char_ind = node_has_next(cur_node, new_str.data[i]);
		//if no path has our character, add it and detach to a new branch
		if(next_char_ind < 0){
			int new_node_ind = dict_add_node(d, node_create(new_str.data[i]));
			cur_node = &d->nodes[cur_node->index];		//re-calc from possible resize
			node_add_next(cur_node, new_node_ind);
			main_branch = cur_node;
			cur_node = &d->nodes[new_node_ind];			//move on to next node
		//if we find a path with our character, follow it
		}else{
			cur_node = &d->nodes[next_char_ind];
		}
	}
}
