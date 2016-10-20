#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../structs.h"
#include "../compress.h"

//### SOME UTILITIES
char_list get_contents(const char * file_name){
	//concat to full path
	char *base = "tests_data/";
	char *path = malloc(strlen(base) + strlen(file_name) + 1);
	strcpy(path, base);
	strcat(path, file_name);
	char_list contents = char_list_init();
	FILE *file = fopen(path, "rb");
	int bytesRead;
	char buffer[1024];
	if (file != NULL){
		while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0){
			char_list_add(&contents, buffer, sizeof(buffer));
		}
	}
	return contents;
}
int assert_int_equals(int a, int b){
	if(a == b){
		printf(".");
		return 0;
	}
	printf("E\n\n");
	return -1;
}
int assert_nodes_equal(dict_node *n1, dict_node *n2, int len){
	for (int i = 0; i < len; i++) {
		if(assert_int_equals(n1[i].value, n2[i].value) < 0){
			printf("Error: Nodes Do Not Match:\n");
			for(int j = 0; j < len; j++) {
				printf("[%c] ", n1[j].value);
			}
			for(int j = 0; j < len; j++) {
				printf("[%c] ", n2[j].value);
			}
			return -1;
		}
	}
	return 0;
}

//### THE ACTUAL TESTS
void css_test1(){
	char_list request = get_contents("css1.request");
	char_list_print(request);
}

void dict_test1(){
	dict d = dict_init();
	//add a string to the dictionnary
	dict_add_string(&d, char_list_from_str("cyka"));
	//nodes should be equal to these
	dict_node nodes1[] = {
		node_create(0),
		node_create('c'),
		node_create('y'),
		node_create('k'),
		node_create('a')
	};
	if(assert_int_equals(d.length, sizeof(nodes1)/sizeof(dict_node)) < 0){
		printf("Length mismatch: %d != %ld\n", d.length, sizeof(nodes1)/sizeof(dict_node));
		return;
	}
	if(assert_nodes_equal(nodes1, d.nodes, d.length) < 0){
		return;
	};
	//add another string, similar to first
	dict_add_string(&d, char_list_from_str("cnyaa"));
	for (int i = 0; i < d.length; i++) {
		printf("[%c] ", d.nodes[i].value);
	}
	printf("\n");
	//check dictionnary again
	dict_node nodes2[] = {
		node_create(0),
		node_create('c'),
		node_create('y'),
		node_create('k'),
		node_create('a'),
		node_create('n'),
		node_create('a')
	};
	if(assert_int_equals(d.length, sizeof(nodes2)/sizeof(dict_node)) < 0){
		printf("Length mismatch: %d != %ld\n", d.length, sizeof(nodes2)/sizeof(dict_node));
		return;
	}
	if(assert_nodes_equal(nodes2, d.nodes, d.length) < 0){
		return;
	};
}

int main(int argc, char const *argv[]) {
	printf("Running Correctness Tests:\n");
	//TODO: correctness
	dict_test1();
	printf("\nRunning Performance Tests:\n");
	//TODO: performance
	return 0;
}
