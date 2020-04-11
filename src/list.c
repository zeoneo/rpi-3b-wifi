#include<kernel/list.h>
#include<mem/kernel_alloc.h>
#include<plibc/string.h>

list * new_list(uint32_t data_size) {
    list *lst = kernel_allocate(sizeof(lst));
    if(lst == NULL) {
        return NULL;
    }
    lst->data_size = data_size;
    lst->list_size = 0;
    lst->head = NULL;
    lst->tail = NULL;
    return lst;
}

void destroy_list(list *lst) {
    l_node *current;
    while(lst->head != NULL) {
        current = lst->head;
        lst->head = current->next;
        kernel_deallocate(current->data);
        kernel_deallocate(current);
    }
    kernel_deallocate(lst);
}

uint32_t list_size(list *lst) {
    return lst->list_size;
}

l_node * list_head(list *lst) {
    if(lst == NULL || lst->head == NULL) {
        return NULL;
    }
    return lst->head; 
}

l_node * list_tail(list *lst) {
    if(lst == NULL || lst->head == NULL) {
        return NULL;
    }
    return lst->head; 
}

void list_push_front(list *lst, void *data) {
    // kernel data structure. use wisely
    l_node *node = kernel_allocate(sizeof(l_node));
    node->data = kernel_allocate(lst->data_size);
    
    memcpy(node->data, data, lst->data_size);

    node->next = lst->head;
    node->prev = NULL; // new head, doesn't have previous element

    if(lst->head != NULL) {
        lst->head->prev = node;
    }

    lst->head = node;

    // first node?
    if(lst->tail == NULL) {
        lst->tail = lst->head;
    }
    lst->list_size++;
}

void list_push_back(list *lst, void *data) {
    // kernel data structure. use wisely
    l_node *node = kernel_allocate(sizeof(l_node));
    node->data = kernel_allocate(lst->data_size);
    
    memcpy(node->data, data, lst->data_size);

    node->next = NULL; // new tail, doesn't have next node
    node->prev = lst->tail;

    if(lst->tail != NULL) {
        lst->tail->next = node;
    }
    
    lst->tail = node; // new tail

    // first node?
    if(lst->head == NULL) {
        lst->head = lst->tail;
    }
    lst->list_size++;
}

void list_delete_head(list *lst) {
    if(lst->head != NULL) {
        l_node *node = lst->head;
        lst->head = node->next;
        if(node->next != NULL) {
            node->next->prev = NULL;
        }
        if(lst->tail == node) {
            lst->tail = NULL;
        }
        kernel_deallocate(node->data);
        kernel_deallocate(node);
        lst->list_size--;
    }
}

void list_delete_tail(list *lst) {
    if(lst->tail != NULL) {
        l_node *node = lst->tail;
        lst->tail = node->prev;
        if(node->prev != NULL) {
            node->prev->next = NULL;
        }
        if(lst->head == node) {
            lst->head = NULL;
        }
        kernel_deallocate(node->data);
        kernel_deallocate(node);
        lst->list_size--;
    }
}

void list_delete_node(list *lst, l_node * node) {
    l_node *prev = node->prev;
    l_node *next = node->next;

    if(prev != NULL) {
        prev->next = next;
    }
    if(next != NULL) {
        next->prev = prev;
    }
    if(lst->head == node) {
        lst->head = next;
    }
    if(lst->tail == node) {
        lst->tail = prev;
    }
    kernel_deallocate(node->data);
    kernel_deallocate(node);
    lst->list_size--;
}

void list_for_each(list *lst, listIterator iterator) {
    if(iterator == 0) {
        return;
    }
    l_node *node = lst->head;
    bool result = true;
    while(node != NULL && result) {
        result = iterator(node);
        node = node->next;
    }
}
