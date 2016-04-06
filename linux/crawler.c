#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <pthread.h>

/*
In the specification for the problem, there is an unbounded queue for downloaders to send work
to parers. The parse_queue implements this unbounded queue. All of the unbounded queue functions
and structures I'm writing with take the form of u_queue_*, to stand for unbounded, and my
downlader queue functions will take the form of b_queue_*, to stand for bounded.
*/

/*
This is the unbounded queue type. Use to send work from downloaders to parsers.
Contains two pointers to nodes, one to point to the front of the queue and one
to point to the end of the queue (back);
Also contains a int size in order to show whether or not the queue is empty or not.
It also contains a single mutex and two condition variables for thread messaging.
*/
typedef struct {
	u_queue_node* front;
	u_queue_node* back;
	int size;
	pthread_mutex_t lock;
	pthread_cond_t full;
	pthread_cond_t empty;
} u_queue;

/*
This is a single node for the unbounded queue type. Has two members:
char* content
and
u_queue_node* next
A string that contains the content of that node and a pointer to the next node in the queue.
*/
struct u_queue_node {
	char* content;
	u_queue_node* next;
}

/*
void u_queue_init: Given an pointer to an uninitialized queue, inits it.

@params:
u_queue* initqueue, The queue to be initialized.
Sets all initial pointers to NULL,
Sets size to 0,
Initializes the lock and condition variables it contains.
*/
void u_queue_init(queue* initqueue)
{
	initqueue->front = NULL;
	initqueue->back = NULL;
	initqueue->size = 0;
	pthread_mutex_init(initqueue->lock);
	pthread_cond_init(initqueue->full);
	pthread_cond_init(initqueue->empty);
}

/*
void u_enqueue: Adds a new node to the end of the queue

@params:
struct u_queue* queue, the queue to be operated on.
char* url, the url that will be added to the back of the queue.
*/
void u_enqueue(struct u_queue* queue, char* url)
{
    struct u_queue_node* newnode;
    newnode = (struct u_queue_node*)malloc(sizeof(struct u_queue_node));
    newnode->content = content;
    newnode->next = NULL;
    stack->back->next = newnode;
		stack->back = newnode;
    stack->size += 1;
}

char* u_dequeue(struct u_queue* queue)
{
}

int isempty(struct u_queue queue)
{
    if (!queue->size)
    {
        return 1;
    }
    return 0;
}

int crawl(char *start_url,
	  int download_workers,
	  int parse_workers,
	  int queue_size,
	  char * (*_fetch_fn)(char *url),
	  void (*_edge_fn)(char *from, char *to))
{
  pthread_t* downloaders = malloc(sizeof(pthread_t) * download_workers);
	pthread_t* parsers = malloc(sizeof(pthread_t) * parse_workers);
	u_queue parse_queue;
	u_queue_init(&parse_queue);
}
