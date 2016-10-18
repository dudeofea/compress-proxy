#ifndef _STRUCTS_H
#define _STRUCTS_H

typedef struct {
  int length;
  int mem_size;
  char * data;
} char_list;

typedef struct dict_node {
  char value;
  struct dict_node *next;
} dict_node;

typedef struct {
  int length;
  int mem_size;
  dict_node *nodes;
} dict;

char_list char_list_init();
void char_list_add(char_list *list, char * new, int new_len);
void char_list_print(char_list list);
char_list char_list_from_str(char * str);
dict dict_init();
void dict_add_node(dict *d, dict_node n);
dict_node make_node(char value);

#endif
