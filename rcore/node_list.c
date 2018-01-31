#include "rebbleos.h"
#include "node_list.h"

void node_add(node_t **head, void *data)
{
    node_t *cur = *head;   
    if (cur == NULL)
    {
        
        cur = malloc(sizeof(node_t));
        assert(cur && "Malloc Failed!");
        cur->data = data;
        cur->next = NULL;
        *head = cur;
        return;
    }
    while (cur->next != NULL)
        cur = cur->next;

    cur->next = malloc(sizeof(node_t));
    assert(cur->next && "Malloc Failed!");
    cur->next->data = data;
    cur->next->next = NULL;
}

void node_insert(node_t **head, node_t *after, void *data)
{
    node_t *cur;
    
    cur = malloc(sizeof(node_t));
    assert(cur && "Malloc Failed!");
    cur->data = data;
    cur->next = NULL;
        
    if (*head == NULL)
    {   
        *head = cur;
        return;
    }
    
    /* insert head */
    if (after == NULL)
    {
        cur->next = *head;
        *head = cur;
        return;
    }
        
    cur->next = after->next;
    after->next = cur;
    window_count();
}

void node_remove(node_t **head, void *data)
{
    node_t *cur = *head;
    node_t *prev;
    
    if (cur != NULL && cur->data == data)
    {
        *head = cur->next;
        free(cur);
                
        return;
    }
    
    /* fast forward to the node we want to remove */
    while(cur != NULL && cur->data != data)
    {
        prev = cur;
        cur = cur->next;
    }
    
    if (cur == NULL)
    {
        SYS_LOG("node", APP_LOG_LEVEL_INFO, "DELETE: Node not found");
        return;
    }
    
    prev->next = cur->next;
    
    free(cur);
}


node_t *node_find(node_t **head, void *data)
{
    node_t *cur = *head;
    
    if (cur != NULL && cur->data == data)
    {
        /* it's the head node */
        return cur;
    }
    
    /* fast forward to the node we want to find */
    while(cur != NULL && cur->data != data)
        cur = cur->next;
    
    if (cur == NULL)
    {
        return NULL;
    }
    
    return cur;
}
