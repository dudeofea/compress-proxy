#ifndef _STRUCTS_H
#define _STRUCTS_H

typedef struct {
  int length;
  int mem_size;
  char * data;
} char_list;

typedef struct dict_node {
  char value;
  int index;
  int branches_size;
  int *branches;
} dict_node;

typedef struct {
  int length;
  int nodes_size;
  dict_node *nodes;
} dict;

char_list char_list_init();
void char_list_add(char_list *list, char * new, int new_len);
void char_list_print(char_list list);
char_list char_list_from_str(char * str);
dict dict_init();
int dict_add_node(dict *d, dict_node n);
void dict_print(dict d);
dict_node node_create(char value);
void node_add_next(dict_node *n, int next);
int node_has_next(dict_node *n, char c);

#endif
