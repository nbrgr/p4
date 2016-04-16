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
This is the struct for the bounded queue, which is used by download_queue. It allows the parsers
to send work to the downloaders.
It has an array that functions as the bounded queue and two integers to track position
int size determines whether or not the queue is empty or full.
It contains a mutex, lock, and two condition variables, full and empty.
*/
typedef struct {
	char** array;
	int front;
	int back;
	int max;
	int size;
	pthread_mutex_t lock;
	pthread_cond_t full;
	pthread_cond_t empty;
} b_queue;

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

typedef struct {
	bucket* next;
	char* link;
} bucket;

typedef struct {
	bucket** table;
	int max;
} hash_table;

/*
void u_queue_init: Given an pointer to an uninitialized queue, inits it.

@params:
u_queue* initqueue, The queue to be initialized.
Sets all initial pointers to NULL,
Sets size to 0,
Initializes the lock and condition variables it contains.
*/
void u_queue_init(u_queue* initqueue)
{
	initqueue->front = NULL;
	initqueue->back = NULL;
	initqueue->size = 0;
	pthread_mutex_init(initqueue->lock);
	pthread_cond_init(initqueue->full);
	pthread_cond_init(initqueue->empty);
}

/*
Initializes a b_queue by setting both front and back positions to 0, size to 0,
allocating the array used as the queue and initializing
the mutex and condition variables using the pthread functions.
*/
void b_queue_init(b_queue* queue, int queue_size)
{
	queue->front = 0;
	queue->back = 0;
	queue->size = 0;
	queue->max = queue_size;
	queue->array = malloc(sizeof(char*) * queue_size);
	pthread_mutex_init(queue->lock);
	pthread_cond_init(queue->full);
	pthread_cond_init(queue->empty);
}

void hash_init(hash_table* tbl, size) {
	tbl->max = size;
	tbl->table = malloc(sizeof(bucket*) * max);
}

/*
The function for the hash table.

@params:
unsigned char *str, the string to be hashed (url)
@return:
unsigned long, the computed key

**NOTE: this is the djb2 function created by Dan Bernstein for strings.**
Source: http://www.cse.yorku.ca/~oz/hash.html
*/
unsigned long hash(unsigned char *str)
{
        unsigned long hash = 5381;
        int c;
    
        while (c = *str++)
            hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

        return hash;
}

int hash_find_insert(hash_table tbl*, char* link) {
        unsigned long key = hash(link) % (tbl->max);
        int found = 0;
        int insert = 0;
        bucket* copy = tbl->table[key];
        
        while(!insert || !found) {
               if(strcmp(copy->link, link) == 0) {
                       found = 1;
               }
               else if(copy->next == NULL) {
                       bucket* new_b = malloc(sizeof(bucket));
                       new_b->next = NULL;
                       new_b->link = link;
                       copy->next = new_b;
                       insert = 1;
               }
               else {
                       copy = copy->next;
               }
        }
        return found;
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
    if (newnode == NULL) {
    	printf(stderr, "Malloc failed\n");
    	exit(1);
    }
    newnode->content = content;
    newnode->next = queue->back;
    queue->back = newnode;
    queue->size += 1;
}

/*
Adds a new node to the end of the b_queue.

@params:
struct b_queue* queue, the queue to add the new node to.
char* url, the url used to later fetch the page content.
*/
void b_enqueue(struct b_queue* queue, char* url)
{
    queue->array[queue->back] = url;
    queue->back++;
    queue->back = queue->back % queue->max;
    queue->size++;
}

/*
char* u_dequeue: Removes the front node of the queue

@params:
struct u_queue* queue, the queue to be operated on
@return:
char*, the content from the removed node (the url)
*/
char* u_dequeue(struct u_queue* queue)
{
    char* url = queue->front->content;
    struct u_queue_node* copy = queue->front;
    queue->front = queue->front->next;
    free(copy);
    return url;
}

char* b_dequeue(struct b_queue* queue)
{
    char* url = queue->array[queue->front];
    queue->front++;
    queue->front = queue->front % queue->max;
    return url;
}

int u_isempty(struct u_queue* queue)
{
    if (!queue->size)
    {
        return 1;
    }
    return 0;
}

int b_isempty(struct b_queue* queue)
{
    if (!queue->size)
    {
	return 1;
    }
    return 0;
}

int b_isfull(struct b_queue* queue)
{
    if (queue->size == queue->max)
    {
	return 1;
    }
    return 0;
}

u_queue parse_queue;
b_queue download_queue;
hash_table* links_visited;
char* from_link = NULL;

void parse_page(char* page, void (*_edge_fn)(char *from, char *to))
{
    char* search = "link:";
    char* save;
    char* found;
    char* token = strtok_r(page, search, &save);
    
    while(token != NULL) {
    	if(strncmp(token,search,5) == 0) {
    		found = strstr(token, search);
    		if(!hash_find_insert(links_visited, found)) {
    		        b_enqueue(&download_queue, found);
    		        _edge_fn(from_link, found);
    		}
    	}
    	token = strtok_r(NULL, search, &save);
    }
}

void* downloader(char* (*_fetch_fn)(char *url))
{
    pthread_mutex_lock(&download_queue->lock);
    while(b_isempty(&download_queue)) {
    	pthread_cond_wait(&download_queue->full, &download_queue->lock);
    }
    char* content = b_dequeue(&download_queue);
    from_link = content;
    content = _fetch_fn(content);
    u_enqueue(&parse_queue, content);

    pthread_cond_signal(&download_queue->empty);
    pthread_mutex_unlock(&download_queue->lock);
}

void* parser(void (*_edge_fn)(char *from, char *to))
{
    pthread_mutex_lock(&parse_queue->lock);
    while(u_isempty(&parse_queue) || b_isfull(&download_queue)) {
    	pthread_cond_wait(&parse_queue->full, &parse_queue->lock);
    }
    char* page = u_dequeue(&download_queue);
    parse_page(page, _edge_fn);

    pthread_cond_signal(&parse_queue->full);
    pthread_mutex_unlock(&parse_queue->lock);
}

int crawl(char *start_url,
	  int download_workers,
	  int parse_workers,
	  int queue_size,
	  char * (*_fetch_fn)(char *url),
	  void (*_edge_fn)(char *from, char *to))
{
    pthread_t downloaders = malloc(sizeof(pthread_t) * download_workers);
    pthread_t parsers = malloc(sizeof(pthread_t) * parse_workers);

    u_queue_init(&parse_queue);
    b_queue_init(&download_queue, queue_size);
    b_enqueue(start_url);
    from_link = start_url;
    hash_init(links_visited, queue_size);
    hash_find_insert(links_visited, start_url);

    int i = 0;
    for(; i < download_workers; i++) {
    	pthread_create(&downloaders[i], NULL, (void*)downloader, (void*)(*_fetch_fn)(char *url));
    }
    for(i = 0; i < parse_workers; i++) {
    	pthread_create(&parsers[i], NULL, (void*)parser, (void*)(*_edge_fn)(char *from, char *to));
    }
    /*for(i = 0; i < download_workers; i++) {
    	pthread_join(downloaders[i], NULL);
    }
    for(i = 0; i < parse_workers; i++) {
    	pthread_join(parsers[i], NULL);
    }*/

    /*while( !(isempty(parse_queue) && isempty(download_queue))  )
    {


    }*/
    return 0;
}
