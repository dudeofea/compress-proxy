#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compress.h"

char_list encode_response(char_list resp, int dict_fd){

}

/* checks if a diictionnary contains a string, if so returns the index */
int dict_contains(dict *d, char *str, int len){
	//naive search
	int matched = 0;
	int str_i = 0;
	for (int i = 0; i < d->length; i++) {
		if(d->nodes[i].value == str[str_i]){
			str_i++;
			matched++;
			if(matched == str_i){
				return i;
			}
		}else{
			//not a match, restart
			matched = 0;
		}
	}
	return -1;
}

/* Add a string to the dictionnary */
void dict_add(dict *d, char_list new_str){
	//TODO: find diff or something
	char_list_print(new_str);
	int ind;
	for (int i = 0; i < new_str.length; i++) {
		//TODO: progressively try out longer and longer substrings of the new string (A)
		//until we no longer find a match (B), at which point we start looking for a substring in the
		//new string which matches a later part of the same string chain that we split off
		//from in the first place (C)
		//
		//		A ——— B ————— C —————>			(existing string)
		//			   \_____/					(new string)
		//
		if((ind = dict_contains(d, &new_str.data[i], 1)) < 0){
			dict_add_node(d, make_node(new_str.data[i]));
		}else{
			//let's see how far the rabbit hole goes...
			dict_node *cur_node = &d->nodes[ind];
			printf("rabbit hole %d, %c\n", ind, cur_node->value);
			// for (int j = i+1; j < new_str.length; j++) {
			// 	if(cur_node->next == NULL || cur_node->value != new_str.data[j]){
			// 		break;
			// 	}
			// }
		}
	}
}
