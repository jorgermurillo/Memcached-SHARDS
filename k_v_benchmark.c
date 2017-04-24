#include "k_v_benchmark.h"
#include "mpscq.h"
#include "mpsc.c"
#include <zmq.h>

// @ Gus: OP QUEUE
typedef struct bm_oq_item_t bm_oq_item_t;
struct bm_oq_item_t {
	bm_op_t op;
	bm_oq_item_t* next;
};

typedef struct bm_oq_t bm_oq_t;
struct bm_oq_t {
	bm_oq_item_t* head;
	bm_oq_item_t* tail;
	pthread_mutex_t lock;
};

static
bm_oq_item_t* malloc_oq_item() {
	bm_oq_item_t* item = malloc(sizeof(bm_oq_item_t));
	return item;
}

static
void free_oq_item(bm_oq_item_t* oq_item) {
	free(oq_item);
}

static
void bm_oq_init(bm_oq_t* oq) {
	pthread_mutex_init(&oq->lock, NULL);
	oq->head = NULL;
	oq->tail = NULL;
}

static
void bm_oq_push(bm_oq_t* oq, bm_oq_item_t* item) {
    item->next = NULL;

    pthread_mutex_lock(&oq->lock);
    if (NULL == oq->tail)
        oq->head = item;
    else
        oq->tail->next = item;
    oq->tail = item;
    pthread_mutex_unlock(&oq->lock);
}

static
bm_oq_item_t* bm_oq_pop(bm_oq_t* oq) {
    bm_oq_item_t* item;

    pthread_mutex_lock(&oq->lock);
    item = oq->head;
    if (NULL != item) {
        oq->head = item->next;
        if (NULL == oq->head)
            oq->tail = NULL;
    }
    pthread_mutex_unlock(&oq->lock);

    return item;
}

// @ Gus: bm settings
bm_type_t bm_type = BM_TO_ZEROMQ;

char bm_output_filename[] = "benchmarking_output.txt";
int  bm_output_fd = -1;

bm_oq_t bm_oq;
struct mpscq* bm_mpsc_oq;
#define BM_MPSC_OQ_CAP 10000 // @ Gus: capacity must be set right becasuse mpsc is NOT a ring buffer
void* zmq_context = NULL;
void* zmq_sender = NULL;

// @ Gus: bm functions
static
bool bm_mpsc_oq_enqueue(bm_op_t op) {
	bm_op_t* op_ptr = malloc(sizeof(bm_op_t));
	*op_ptr = op;
	return mpscq_enqueue(bm_mpsc_oq, op_ptr);
}

static
void bm_init() {
	if (bm_type == BM_NONE) return;
	fprintf(stderr, "----------------------->GUS: Init Benchmarking\n");
	switch(bm_type) {
    	case BM_NONE: {
    		;
    	} break;
    	case BM_PRINT: {
    		;
    	} break;
    	case BM_DIRECT_FILE: {
    		bm_output_fd = open(bm_output_filename, 
    						 O_WRONLY | O_CREAT | O_TRUNC,
    						 S_IRUSR, S_IWUSR);
    	} break;
    	case BM_TO_QUEUE: {
    		bm_oq_init(&bm_oq);
    	} break;
    	case BM_TO_LOCK_FREE_QUEUE: {
    		bm_mpsc_oq = mpscq_create(NULL, BM_MPSC_OQ_CAP);
    	} break;
    	case BM_TO_ZEROMQ: {
		    zmq_context = zmq_ctx_new ();
		    zmq_sender = zmq_socket (zmq_context, ZMQ_PUB);
		    int rc = zmq_bind(zmq_sender, "tcp://*:5555");
		    fprintf (stderr, "Started zmq server...\n");
    	} break;
    }
}

static
void bm_write_line_op(int fd, bm_op_t op) {
	size_t str_buffer_length = 3 + 10;
	char* str_buffer = malloc(str_buffer_length);
	sprintf(str_buffer, "%d %"PRIu64"\n", op.type, op.key_hv);
	write(fd, str_buffer, strlen(str_buffer));
	free(str_buffer);
}

static
void bm_write_op_to_oq(bm_oq_t* oq, bm_op_t op) {
	bm_oq_item_t* item = malloc_oq_item();
	item->op = op;
	bm_oq_push(oq, item);
}

static
void bm_process_op(bm_op_t op) {
	// bm_write_line_op(bm_output_fd, op);
	// fprintf(stderr, "type: %d, key: %"PRIu64"\n", op.type, op.key_hv);
}

static
void bm_consume_ops() {
	switch(bm_type) {
		case BM_NONE: {
    		;
    	} break;
    	case BM_PRINT: {
    		;
    	} break;
    	case BM_DIRECT_FILE: {
 			;
    	} break;
    	case BM_TO_QUEUE: {
    		bm_oq_item_t* item = bm_oq_pop(&bm_oq);
			while(NULL != item) {
				bm_process_op(item->op);
				free_oq_item(item);
				item = bm_oq_pop(&bm_oq);
			}
    	} break;
    	case BM_TO_LOCK_FREE_QUEUE: {
    		void* item = mpscq_dequeue(bm_mpsc_oq);
    		while(NULL != item) {
    			bm_op_t* op_ptr = item;
    			bm_process_op(*op_ptr);
    			free(op_ptr);
    			item = mpscq_dequeue(bm_mpsc_oq);
    		}
    	} break;
    	case BM_TO_ZEROMQ: {
    		;
    	} break;
	}
}

// @ Gus: LIBEVENT
static
struct event_base* bm_event_base;

static
void bm_clock_handler(evutil_socket_t fd, short what, void* args) {
	// fprintf(stderr, "Tick\n");
	bm_consume_ops();
}

static
void bm_libevent_loop() {
	bm_event_base = event_base_new();

	struct event* timer_event = event_new(bm_event_base,
								   -1,
								   EV_TIMEOUT | EV_PERSIST,
								   bm_clock_handler,
								   NULL);
	struct timeval t = {2, 0};
    event_add(timer_event, &t);

	event_base_dispatch(bm_event_base);
}

static
void* bm_loop_in_thread(void* args) {
	bm_output_fd = open(bm_output_filename, 
						O_WRONLY | O_CREAT | O_TRUNC,
						S_IRUSR | S_IWUSR);
	bm_libevent_loop();
	// while(true) bm_consume_ops();
	return NULL;
}

static
void bm_record_op(bm_op_t op) {
    char* command = op.type == BM_READ_OP ? "GET" : "SET";
    switch(bm_type) {
        case BM_NONE: {
            ;
        } break;
        case BM_PRINT: {
            fprintf(stderr, "----------------------->GUS: PROCESS %s COMMAND WITH KEY HASH: %"PRIu64"\n", command, op.key_hv);
        } break;
        case BM_DIRECT_FILE: {
            bm_write_line_op(bm_output_fd, op);
        } break;
        case BM_TO_QUEUE: {
            bm_write_op_to_oq(&bm_oq, op);
        } break;
        case BM_TO_LOCK_FREE_QUEUE: {
            bm_mpsc_oq_enqueue(op);
        } break;
        case BM_TO_ZEROMQ: {
            // fprintf(stderr, "sending op: %d, hv: %"PRIu64"\n", op.type, op.key_hv);
            zmq_send(zmq_sender, &op, sizeof(bm_op_t), ZMQ_DONTWAIT);
        } break;
    }
}
/*
static
void bm_record_read_op(char* key, size_t key_length) {
	bm_op_t op = {BM_READ_OP, hash(key, key_length)};
    switch(bm_type) {
    	case BM_NONE: {
    		;
    	} break;
    	case BM_PRINT: {
    		fprintf(stderr, "----------------------->GUS: PROCESS GET COMMANDD WITH KEY: %s (%"PRIu64")\n", key, op.key_hv);
    	} break;
    	case BM_DIRECT_FILE: {
 			bm_write_line_op(bm_output_fd, op);
    	} break;
    	case BM_TO_QUEUE: {
    		bm_write_op_to_oq(&bm_oq, op);
    	} break;
    	case BM_TO_LOCK_FREE_QUEUE: {
    		bm_mpsc_oq_enqueue(op);
    	} break;
    	case BM_TO_ZEROMQ: {
    		// fprintf(stderr, "sending op: %d, hv: %"PRIu64"\n", op.type, op.key_hv);
    		zmq_send(zmq_sender, &op, sizeof(bm_op_t), ZMQ_DONTWAIT);
    	} break;
    }
}

static
void bm_record_write_op(char* command, char* key, size_t key_length) {
	bm_op_t op = {BM_WRITE_OP, hash(key, key_length)};
	switch(bm_type) {
		case BM_NONE: {
    		;
    	} break;
    	case BM_PRINT: {
    		fprintf(stderr, "----------------------->GUS: PROCESS %s COMMANDD WITH KEY: %s (%"PRIu64")\n", command, key, op.key_hv);
    	} break;
    	case BM_DIRECT_FILE: {
    		bm_write_line_op(bm_output_fd, op);
    	} break;
    	case BM_TO_QUEUE: {
    		bm_write_op_to_oq(&bm_oq, op);
    	} break;
    	case BM_TO_LOCK_FREE_QUEUE: {
    		bm_mpsc_oq_enqueue(op);
    	} break;
    	case BM_TO_ZEROMQ: {
    		// fprintf(stderr, "sending op: %d, hv: %"PRIu64"\n", op.type, op.key_hv);
    		zmq_send(zmq_sender, &op, sizeof(bm_op_t), ZMQ_DONTWAIT);
    	} break;
    }
}
*/