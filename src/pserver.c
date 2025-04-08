#include <stdio.h>
#include "request.h"
#include "io_helper.h"
#include "pthread.h"

#define FIFO 0
#define SFF 1
#define MAXBUF (8192)

char default_root[] = ".";
int default_schedalg = FIFO;
int default_port = 10000;
int default_threads = 1;
int default_buffers = 1;

//------------------------------------------------------------------------
// Thread pool struct & Node singly LL struct
//------------------------------------------------------------------------

struct Pool {
	int maxConn; // max connections set by user
	int maxElements; // max elements in buffer 
	int countElements;
	int schedalg;

	pthread_mutex_t lock;
	pthread_t *threads; // array of worker threads
	struct Node *dummy; // dummy node that points to first element in LL

	pthread_cond_t buff_full; // producer block & wait is buffer is full
	pthread_cond_t buff_empty; // worker wait if buffer is empty
};

struct Node {
	int fd; // connection fd from client
	size_t file_size; // size of the file
	struct Node *next; // points to the next node
};

//------------------------------------------------------------------------
// Linked list helper functions
//------------------------------------------------------------------------

struct Node* create_node(int fd, size_t file_size) {
	struct Node *node = malloc(sizeof(struct Node));
	if (node == NULL) {
		fprintf(stderr, "Malloc failed creating new node\n");
		exit(1);
	}
	node->fd = fd;
	node->file_size = file_size;
	node->next = NULL;
	return node;
}

void append_node(struct Pool *pool, int fd, size_t file_size) {
	struct Node *node = create_node(fd, file_size);

	// adding node to end of list
	if (pool->dummy->next == NULL) {
		pool->dummy->next = node;
	} else {
		struct Node *current = pool->dummy->next;
		while (current->next != NULL)
			current = current->next;
		current->next = node;
	}
	pool->countElements++; 
}

// update sffNode, update pointers, return ssfNode
struct Node* get_sff_node(struct Pool *pool) {
    struct Node *min_prev = pool->dummy;            
    struct Node *cur_prev = pool->dummy;              

    while (cur_prev->next != NULL) {                 
        if (cur_prev->next->file_size < min_prev->next->file_size) {
            min_prev = cur_prev;                     
        }
        cur_prev = cur_prev->next;
    }

    struct Node *sff_node = min_prev->next;
	// Dummy -> N1 -> N2 (min_prev) -> N3 (min_node) -> N4 (next) -> NULL
	// Dummy -> N1 -> N2 (min_prev) -> N4 (next) -> NULL
    min_prev->next = sff_node->next;
    pool->countElements--;
    return sff_node;
}

struct Node* get_fifo_node(struct Pool *pool) {
	struct Node *fifo_node = pool->dummy->next;
	
	// Updating head of the node 
	// Dummy -> N1 -> N2 -> N3 -> NULL
	// Dummy -> N2 -> N3 -> NULL
	struct Node *next = fifo_node->next;
	pool->dummy->next = next;
	pool->countElements--;

	return fifo_node;
}

//------------------------------------------------------------------------

void *consumer(void *arg) {
    struct Pool *pool = (struct Pool *)arg;

	while (1) {
		pthread_mutex_lock(&pool->lock);
		while (pool->countElements == 0) {
			pthread_cond_wait(&pool->buff_full, &pool->lock);
		}
		struct Node *node;
		if (pool->schedalg == FIFO) {
			node = get_fifo_node(pool);
		} else {
			node = get_sff_node(pool);
		}
		pthread_cond_signal(&pool->buff_empty);
		pthread_mutex_unlock(&pool->lock);

		request_handle(node->fd);
        close_or_die(node->fd);
        free(node);
	}
}

void initpool(struct Pool *pool, int num_threads, int num_elements, int schedalg) {
	pool->maxConn = num_threads;
	pool->maxElements = num_elements;
	pool->countElements = 0;
	pool->schedalg = schedalg;

	// allocating mem for a dummy node that points to the front of the LL
	pool->dummy = malloc(sizeof(struct Node)); 
	pool->dummy->fd = -1;
	pool->dummy->next = NULL;

	pthread_mutex_init(&pool->lock, NULL);
	pool->threads = malloc(sizeof(pthread_t) * num_threads);
	if (pool->threads == NULL) {
		fprintf(stderr, "Malloc failed creating array for threads\n");
		exit(1);
	}
	pthread_cond_init(&pool->buff_full, NULL);
	pthread_cond_init(&pool->buff_empty, NULL);

	// Creating a pool of worker threads
	for (int i = 0; i < pool->maxConn; ++i) {
		pthread_create(&pool->threads[i], NULL, consumer, pool);
	}
}

// main() is the producer
// ./pserver [-d basedir] [-p port] [-t threads] [-b buffers] [-s schedalg]
int main(int argc, char *argv[]){
	int c;
	char *root_dir = default_root;
	int port = default_port;
	int threads = default_threads;
	int buffers = default_buffers;
	int schedalg = default_schedalg;

	while((c = getopt(argc, argv, "d:p:t:b:s:")) != -1)
		switch(c){
		case 'd':
			root_dir = optarg;
			break;
		case 'p':
			port = atoi(optarg);
			break;
		case 't':
			threads = atoi(optarg);
			if (threads < 0) {
				fprintf(stderr, "Threads: %d must be positive\n", threads);
				exit(1);
			}
			break;
		case 'b':
			buffers = atoi(optarg);
			if (buffers < 0){
				fprintf(stderr, "Elements: %d must be postive\n", buffers);
				exit(1);
			}
			break;
		case 's':
			if (strcmp(optarg, "SFF") == 0) {
				schedalg = SFF;
			} else {
				schedalg = FIFO;
			}
			break;
		default:
			fprintf(stderr, "usage: pserver [-d basedir] [-p port] [-t threads] [-b buffers] [-s schedalg]\n");
			exit(1);
		}

	// run out of this directory
	chdir_or_die(root_dir);

	// initialize the connection pool
	struct Pool pool;
	initpool(&pool, threads, buffers, schedalg);

	// now, get to work
	int listen_fd = open_listen_fd_or_die(port);
	while(1){
		struct sockaddr_in client_addr;
		int client_len = sizeof(client_addr);
		int conn_fd = accept_or_die(listen_fd, (sockaddr_t *)&client_addr, (socklen_t *)&client_len);

		int is_static;
		size_t file_size;
		struct stat sbuf;
		char buf[MAXBUF], method[MAXBUF], uri[MAXBUF], version[MAXBUF];
		char filename[MAXBUF], cgiargs[MAXBUF];

		readline_or_die(conn_fd, buf, MAXBUF);
		sscanf(buf, "%s %s %s", method, uri, version);
		request_read_headers(conn_fd);

		is_static = request_parse_uri(uri, filename, cgiargs);
		if (stat(filename, &sbuf) == 0) {
			file_size = sbuf.st_size;	
		} else {
			request_error(conn_fd, filename, "404", "Not found", "server could not find this file");
			close_or_die(conn_fd);
			continue;
		}


		pthread_mutex_lock(&pool.lock);
		// releases lock and goes to sleep while buf is full
		while (pool.countElements == pool.maxElements) {
			pthread_cond_wait(&pool.buff_empty, &pool.lock);
		}

		append_node(&pool, conn_fd, file_size);
		pthread_cond_signal(&pool.buff_full);
		pthread_mutex_unlock(&pool.lock);
	}
	return 0;
}

