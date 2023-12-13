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

// Assumes queue is not Null
pthread_t* dequeue(struct thread_queue* queue) {

    if (isNull(queue)) {
        return NULL;
    }

    struct thread_node *node_to_rm = queue->tail;
    pthread_t* thread_return = node_to_rm->thread;

    if (queue->head == queue->tail) {
        queue->head = NULL;
        queue->tail = NULL;
        free(node_to_rm);
        return thread_return;
    }

    queue->tail = node_to_rm->prev;
    (queue->tail)->next = NULL;

    free(node_to_rm);
    return thread_return;
}

bool isNull(struct thread_queue* queue) {
    return (queue->head == NULL);
}

void init_queue(struct thread_queue* queue) {
    queue->head = NULL;
    queue->tail = NULL;
}