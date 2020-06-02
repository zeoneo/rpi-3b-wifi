
#ifndef LIST_H_
#define LIST_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef bool (*listIterator)(void*);

typedef struct l_node l_node;

struct l_node {
    void* data;
    l_node* next;
    l_node* prev;
};

typedef struct {
    int list_size;
    int data_size;
    l_node* head;
    l_node* tail;
} list;

list* new_list(uint32_t data_size);
void destroy_list(list* lst);
uint32_t list_size(list* lst);

l_node* list_head(list* lst);
l_node* list_tail(list* lst);

void list_push_front(list* lst, void* data);
void list_push_back(list* lst, void* data);

void list_delete_head(list* lst);
void list_delete_tail(list* lst);
void list_delete_node(list* lst, l_node* node);

void list_for_each(list* list, listIterator iterator);

#ifdef __cplusplus
}
#endif

#endif // LIST_H_