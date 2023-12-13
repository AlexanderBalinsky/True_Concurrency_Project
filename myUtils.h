#include <pthread.h>

#ifndef myUtils
#define myUtils

#include <stdbool.h>
#include <stdlib.h>

//NOT THREAD SAFE

struct thread_node {
    pthread_t* thread;
    struct thread_node * prev;
    struct thread_node * next;
};

//ll = linked list
struct thread_queue {
    struct thread_node* head;
    struct thread_node* tail;
};

bool enqueue(struct thread_queue*, pthread_t*, struct thread_node *);

pthread_t* dequeue(struct thread_queue*);

bool isNull(struct thread_queue*);

void init_queue(struct thread_queue*);

#endif