#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <pthread.h>

//Forward declarations:
struct u_queue_node;
struct bucket;
struct hashtable;
struct u_queue;
struct b_queue;

typedef struct u_queue_node u_queue_node;
typedef struct bucket bucket;
typedef struct hashtable hashtable;
typedef struct u_queue u_queue;
typedef struct b_queue b_queue;

int interrupted_u_enqueue = 0;

void u_queue_init(u_queue* initqueue);
void b_queue_init(b_queue* queue, int queue_size);
unsigned long hash(char *str);
void hash_init(hashtable *tbl, int size);
int hash_find_insert(hashtable *tbl, char* link);
int u_enqueue(u_queue* queue, char* url, char* page);
void b_enqueue(b_queue* queue, char* url);
u_queue_node* u_dequeue(u_queue* queue);
int u_isempty(u_queue* queue);
int b_isfull(b_queue* queue);

/*
This is a single node for the unbounded queue type. Has two members:
char* content
and
u_queue_node* next
A string that contains the content of that node and a pointer to the next node in the queue.
*/
struct u_queue_node {
    char* content;
    char* from_link;
    u_queue_node* next;
    u_queue_node* prev;
};

struct bucket {
    bucket* next;
    char* link;
};

struct hashtable {
    bucket** table;
    int max;
    pthread_mutex_t* lock;
};

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
struct u_queue {
	u_queue_node* front;
	u_queue_node* back;
	int size;
	pthread_mutex_t* lock;
	pthread_cond_t* empty;
} ;

/*
This is the struct for the bounded queue, which is used by download_queue. It allows the parsers
to send work to the downloaders.
It has an array that functions as the bounded queue and two integers to track position
int size determines whether or not the queue is empty or full.
It contains a mutex, lock, and two condition variables, full and empty.
*/
struct b_queue {
	char** array;
	int front;
	int back;
	int max;
	int size;
	pthread_mutex_t* lock;
	pthread_cond_t* empty;
	pthread_cond_t* full;
};

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
	initqueue->lock = malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(initqueue->lock, NULL);
	initqueue->empty = malloc(sizeof(pthread_cond_t));
	pthread_cond_init(initqueue->empty, NULL);
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
	queue->lock = malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(queue->lock, NULL);
	queue->empty = malloc(sizeof(pthread_cond_t));
	pthread_cond_init(queue->empty, NULL);
	queue->full = malloc(sizeof(pthread_cond_t));
	pthread_cond_init(queue->full, NULL);
}

void hash_init(hashtable* tbl, int size) {
	tbl->max = size;
	tbl->table = malloc(sizeof(bucket) * size);
	tbl->lock = malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(tbl->lock, NULL);
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
unsigned long hash(char *str)
{
        unsigned long hash = 5381;
        int c;
        int i;
        for (i = 0; str[i] != 0; i++)
        {
            c = str[i];
            hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
        }

        return hash;
}

int hash_find_insert(hashtable *tbl, char* link) {
	
	printf("start insert\n");
        unsigned long key = hash(link) % (tbl->max);
        printf("key: %li\n", key);
        int found = 0;
        int insert = 0;
        
        if(tbl->table[key] == NULL) {
        	tbl->table[key] = malloc(sizeof(bucket));
        	(tbl->table[key])->next = NULL;
        	(tbl->table[key])->link = malloc(sizeof(char) * (int)strlen(link));
        	(tbl->table[key])->link = strcpy((tbl->table[key])->link, link);
        	if( *((tbl->table[key])->link + (int)strlen(link) - 1) != '\0') {
        		*((tbl->table[key])->link + (int)strlen(link) - 1) = '\0';
        	} 
        	printf("bucket: %s\n", (tbl->table[key])->link);
        }
        else {
        	bucket* copy = tbl->table[key];
                while(!insert && !found) {
                       if(strcmp(copy->link, link) == 0) {
                               found = 1;
                       }
                       else if(copy->next == NULL) {
                               bucket* new_b = malloc(sizeof(bucket));
                               new_b->next = NULL;
                               new_b->link = link;
                               printf("bucket: %s\n", new_b->link);
                               copy->next = new_b;
                               insert = 1;
                        }
                        else {
                               copy = copy->next;
                        }
                }
        }
        printf("end insert\n");
        return found;
}

/*
void u_enqueue: Adds a new node to the end of the queue

@params:
struct u_queue* queue, the queue to be operated on.
char* url, the url used to find the page contents.
char* page, the page contents that will be parsed.
*/
int u_enqueue(struct u_queue* queue, char* url, char* page)
{
    if(queue == NULL || url == NULL) { return -1; }
    struct u_queue_node* newnode;
    newnode = (struct u_queue_node*)malloc(sizeof(struct u_queue_node));
    newnode->content = malloc(sizeof(char) * (int)strlen(page));
    newnode->from_link = malloc(sizeof(char) * (int)strlen(url));
    newnode->next = malloc(sizeof(u_queue_node));
    newnode->prev = malloc(sizeof(u_queue_node));
    if (newnode == NULL) {
    	fprintf(stderr, "Malloc failed\n");
    	return -1;
    }
    queue->size++;
    newnode->content = strcpy(newnode->content, page);
    if( *(newnode->content + (int)strlen(newnode->content) - 1) != '\0') {
    	*(newnode->content + (int)strlen(newnode->content) - 1) = '\0';
    }
    newnode->from_link = strcpy(newnode->from_link, url);
    if( *(newnode->from_link + (int)strlen(newnode->from_link) - 1) != '\0') {
    	*(newnode->from_link + (int)strlen(newnode->from_link) - 1) = '\0';
    }
    if(queue->size == 1 || (queue->size == 2 && interrupted_u_enqueue) ) {
    	 printf("DEAD END SHIIIIT\n");
    	 newnode->next = NULL;
    	 newnode->prev = NULL;
    	 queue->front = newnode;
    	 queue->back = newnode;
    }
    else {
    	newnode->next = queue->back;
    	queue->back->prev = newnode;
    	newnode->prev = NULL;
    	queue->back = newnode;
    }
    return 0;
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
u_queue_node*, the removed node
*/
u_queue_node* u_dequeue(struct u_queue* queue)
{
    struct u_queue_node* copy = queue->front;
    queue->front = queue->front->prev;
    if(queue->front != NULL) {
    	queue->front->next = NULL;
    }
    queue->size--;
    if(u_isempty(queue)) {
    	queue->back = NULL;
    }
    return copy;
}

char* b_dequeue(struct b_queue* queue)
{
    char* url = queue->array[queue->front];
    queue->front++;
    queue->front = queue->front % queue->max;
    queue->size--;
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

u_queue* parse_queue;
b_queue* download_queue;
hashtable* links_visited;
int work_count = 0;
int work_completed = 0;

pthread_mutex_t* lock;
pthread_cond_t* not_done;

void parse_page(u_queue_node* node, void (*_edge_fn)(char *from, char *to))
{
    printf("parse_page\n");
    char* search = "link:";
    char* save;
    char* found;
    int hash_result;
    int ever_interrupted = 0;
    int offset = 0;
    int prev = parse_queue->size;
    char* copy;
    
    do {
    	copy = malloc(sizeof(char) * ( (int)strlen(node->content) - offset) );
    	printf("gonna copy\n");
    	copy = strcpy(copy, node->content + offset);
    	/*if( *(copy + (int)strlen(copy) - 1) != '\0') {
    		*(found + (int)strlen(copy) - 1) = '\0';
    	}*/
    	printf("copy: %s\n", copy);
    	char* token = strtok_r(copy, " \n", &save);
    	printf("token interrupted: %s\n", token);
    	printf("offset: %i\n", offset);
    	interrupted_u_enqueue = 0;
    	while(token != NULL && !interrupted_u_enqueue) {
    		if(strncmp(token, search, 5) == 0) {
    			while(download_queue->size >= download_queue->max) {
    				if(prev >= parse_queue->size) {
    					parse_queue->size++;
    				}
    				ever_interrupted = 1;
    				interrupted_u_enqueue = 1;
    				//pthread_cond_signal(parse_queue->empty);
    				//pthread_cond_signal(download_queue->empty);
    				printf("waiting interrupted execution\n");
    				pthread_cond_wait(download_queue->full, download_queue->lock);
    			}
    			if(!interrupted_u_enqueue) {
    				found = malloc(sizeof(char) * ((int)strlen(token) - 5) );
    				found = strcpy(found, token + 5);
    				if( *(found + (int)strlen(found) - 1) != '\0' && *(found + (int)strlen(found) - 1) == '%') {
    					*(found + (int)strlen(found) - 1) = '\0';
    				}
	 			printf("found link: %s\n", found);
    				pthread_mutex_lock(links_visited->lock);
    				hash_result = hash_find_insert(links_visited, found);
    				pthread_mutex_unlock(links_visited->lock);
    				if(!hash_result) {
    					work_count++;
    				        b_enqueue(download_queue, found);
    				        printf("from: %s, to: %s\n", node->from_link, found);
	   		        	pthread_mutex_lock(lock);
    				        _edge_fn(node->from_link, found);
    				        pthread_mutex_unlock(lock);
    				}
    			}
    		
    		}
    		if(!interrupted_u_enqueue) {
    			token = strtok_r(NULL, " \n", &save);
    			if(token != NULL) {
    				offset += (int)strlen(token);
    			}
    		}
    		
    		printf("token: %s\n", token);
    		printf("offset: %i\n", offset);
    	}
    	printf("interrupted: %i\n", interrupted_u_enqueue);
    } while(interrupted_u_enqueue);
    
    if(ever_interrupted) {
    	parse_queue->size--;
    }
    
    printf("end parse_page\n");
    work_completed++;
}

void downloader(char* (*_fetch_fn)(char *url))
{
    
    while(work_count != work_completed)
    {
    	printf("count: %i, complete: %i\n", work_count, work_completed);
        pthread_mutex_lock(download_queue->lock);
        printf("start downloader\n");
        while(b_isempty(download_queue)) {
        	printf("waiting download_queue empty\n");
        	if(u_isempty(parse_queue)) {
        		pthread_cond_signal(not_done);
        	}
        	pthread_cond_wait(download_queue->empty, download_queue->lock);
        }
        printf("Testing seg: download_queue: %i, parse_queue: %i\n", download_queue->size, parse_queue->size);
        char* url = b_dequeue(download_queue);
        printf("link to fetch: %s\n", url);
        pthread_mutex_lock(lock);
        char* page = _fetch_fn(url);
        pthread_mutex_unlock(lock);
        printf("fetched: %s\n", url);
        u_enqueue(parse_queue, url, page);
        printf("u_enqueue page\n");

        if(u_isempty(parse_queue) && b_isempty(download_queue)) {
        	pthread_cond_signal(not_done);
        }

        pthread_cond_signal(parse_queue->empty);
        pthread_cond_signal(download_queue->full);
        printf("end downloader\n");
        printf("download_queue: %i, parse_queue: %i\n", download_queue->size, parse_queue->size);
        pthread_mutex_unlock(download_queue->lock);
    }
}

void parser(void (*_edge_fn)(char *from, char *to))
{
    printf("parser begin\n");
    while(work_count != work_completed) {
    	printf("count: %i, complete: %i\n", work_count, work_completed);
        pthread_mutex_lock(parse_queue->lock);
        printf("start parser\n");
        while(u_isempty(parse_queue)) {
        	printf("wait empty parse queue\n");
        	pthread_cond_signal(download_queue->empty);
        	pthread_cond_wait(parse_queue->empty, parse_queue->lock);
        }
        printf("download_queue: %i, parse_queue: %i\n", download_queue->size, parse_queue->size);
        u_queue_node* node = u_dequeue(parse_queue);
        printf("page: %s\n", node->content);
        printf("u_dequeue done\n");
        pthread_mutex_unlock(parse_queue->lock);
        
        pthread_mutex_lock(download_queue->lock);
        while(b_isfull(download_queue)) {
            printf("wait full download queue\n");
    	    pthread_cond_wait(download_queue->full, download_queue->lock);
        }
        parse_page(node, _edge_fn);

        if(u_isempty(parse_queue) && b_isempty(download_queue)) {
        	pthread_cond_signal(not_done);
        }

        pthread_cond_signal(download_queue->empty);
        printf("end parser\n");
        printf("download_queue: %i, parse_queue: %i\n", download_queue->size, parse_queue->size);
        pthread_mutex_unlock(download_queue->lock);
    }
}

int crawl(char *start_url,
	  int download_workers,
	  int parse_workers,
	  int queue_size,
	  char * (*_fetch_fn)(char *url),
	  void* (*_edge_fn)(char *from, char *to))
{
    printf("start crawl\n");
    pthread_t* downloaders = malloc(sizeof(pthread_t) * download_workers);
    pthread_t* parsers = malloc(sizeof(pthread_t) * parse_workers);
    parse_queue = malloc(sizeof(u_queue));
    download_queue = (b_queue*)malloc(sizeof(b_queue));
    links_visited = malloc(sizeof(hashtable));
    
    lock = malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(lock, NULL);
    not_done = malloc(sizeof(pthread_cond_t));
    pthread_cond_init(not_done, NULL);

    u_queue_init(parse_queue);
    b_queue_init(download_queue, queue_size);
    b_enqueue(download_queue, start_url);
    work_count++;
    printf("page: %s\n", start_url);
    printf("first link added\n");
    hash_init(links_visited, queue_size);
    hash_find_insert(links_visited, start_url);

    int i = 0;
    for(; i < download_workers; i++) {
    	pthread_create(&downloaders[i], NULL, (void*)downloader, (void*)_fetch_fn);
    }
    printf("%i downloader threads\n", i);
    for(i = 0; i < parse_workers; i++) {
    	pthread_create(&parsers[i], NULL, (void*)parser, (void*)_edge_fn);
    }
    printf("%i parser threads\n", i);
    
    if(work_count != work_completed) {
    	printf("MAAAAAAIIIIIIIIIIINNNNNNN\n");
    	pthread_mutex_lock(lock);
    	pthread_cond_wait(not_done, lock);
    	pthread_cond_signal(parse_queue->empty);
    	//pthread_cond_signal(download_queue->empty);
    	pthread_cond_signal(download_queue->full);
    	pthread_mutex_unlock(lock);
    }
    
    /*for(i = 0; i < download_workers; i++) {
    	pthread_join(downloaders[i], NULL);
    }
    for(i = 0; i < parse_workers; i++) {
    	pthread_join(parsers[i], NULL);
    }*/

    printf("end crawl\n");
    
    //pthread_exit(0);
    exit(0);
}
