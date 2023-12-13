#include "myUtils.h"

bool enqueue(struct thread_queue* queue, pthread_t* thread, 
            struct thread_node *new_node) {
    new_node->thread = thread;
    new_node->prev = NULL;

    if (queue->head == NULL) {
        queue->head = new_node;
        queue->tail = new_node;
        new_node->next = NULL; 
        return true;
    } else {
        struct thread_node *old_head = queue->head;
        old_head->prev = new_node;
        new_node->next = old_head;
        queue->head = new_node;
        return true;
    }
}

struct thread_node *dequeue(struct thread_queue* queue) {

    if (isNull(queue)) {
        return NULL;
    }

    struct thread_node *node_to_rm = queue->tail;

    if (queue->head == queue->tail) {
        queue->head = NULL;
        queue->tail = NULL;
        return node_to_rm;
    }

    queue->tail = node_to_rm->prev;
    (queue->tail)->next = NULL;

    node_to_rm->prev = NULL;
    node_to_rm->next = NULL;

    return node_to_rm;
}

bool isNull(struct thread_queue* queue) {
    return (queue->head == NULL);
}

void init_queue(struct thread_queue* queue) {
    queue->head = NULL;
    queue->tail = NULL;
}