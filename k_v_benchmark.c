#include "k_v_benchmark.h"
#include "memcached.h"
#include "shards_monitor.h"
//#include "mpscq.h"
#include "SHARDS.h"
#include <zmq.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
//#include "waitfree-mpsc-queue/mpscq.h"
//#include "waitfree-mpsc-queue/mpsc.c"
//#include "ringbuf/src/ringbuf.h"
//#include "ringbuf/src/ringbuf.c"
#include "liblfds7.1.1/liblfds7.1.1/liblfds711/inc/liblfds711.h" 

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
bm_type_t bm_type = BM_NONE;
//bm_type_t bm_type = BM_TO_QUEUE; 
//bm_type_t bm_type = BM_TO_ZEROMQ;
//bm_type_t bm_type = BM_TO_LOCK_FREE_QUEUE;

bm_oq_t bm_oq;

bm_process_op_t bm_process_op_type = BM_PROCESS_DUMMY;//BM_PROCESS_PRINT; //
int SPIN_TIME = -1;
int random_accum = 0;


/*
//Ring-Buffer stuff
#define MAX_WORKERS 2
//static size_t ringbuf_obj_size;
static size_t ringbuf_obj_size = 0;
ringbuf_t* bm_ringbuf;
ringbuf_worker_t* w1;
ssize_t off1 = -1;
unsigned char* buf;
*/

char bm_output_filename[] = "benchmarking_output.txt";
int  bm_output_fd = -1;



//MPSC (Lockfree queue) stuff
//struct mpscq* bm_mpsc_oq;
int QUEUE_LENGTH = (1500000 +1);// 


//Ringbuf stuff
enum lfds711_misc_flag overwrite_occurred_flag;
struct lfds711_ringbuffer_element *re;
struct lfds711_ringbuffer_state rs;


//ZeroMQ stuff
void* zmq_context = NULL;
void* zmq_sender = NULL;
pthread_mutex_t zeroMQ_lock;

// @ Gus: bm functions
/*
static
bool bm_mpsc_oq_enqueue(bm_op_t op) {
	bm_op_t* op_ptr = malloc(sizeof(bm_op_t));
	*op_ptr = op;
	return mpscq_enqueue(bm_mpsc_oq, op_ptr);
}
*/
int get_and_set_config_from_file() {
	
	static char* filename = "./bm_config.txt";
	FILE* bm_config_fptr = fopen(filename, "r");
	if (bm_config_fptr == NULL) {
		fprintf(stderr, "%s does NOT exist or it is Wrong.\n", filename);
		return -1;
	}
	fprintf(stderr,"Config File functio.\n");
	char line[50];
	fgets(line, 50, bm_config_fptr);
	bm_type = atoi(line);					// Queue Type

	fgets(line, 50, bm_config_fptr);
	QUEUE_LENGTH = atoi(line);				// Queue Length



	/*
	fgets(line, 50, bm_config_fptr);
	bm_process_op_type = atoi(line);
	fgets(line, 50, bm_config_fptr);
	SPIN_TIME = atoi(line);
	fclose(bm_config_fptr);
	*/

	return 0;
}

void bm_init(int epoch_length, bm_type_t queue_type, uint32_t *slab_sizes, double factor, double R_initialize) {
	
	int rc = get_and_set_config_from_file();

	if(rc!=0){
		fprintf(stderr, "The configuration file bm_cofig.txt could not be found. Using default and/or flag values.\n");
	}else{
	}
	// If queue_type has a value of -1, it means that no flag option was given for the type of queue, and we need to use the value in the config file.
	// The validation of wheter a value was given, and if the value was in the desired range is done in the main. 
	if(queue_type!=-1){
		bm_type = queue_type;
	}

	// Fom here on we use the variable bm_type to set set up the corresponding type of queue
	fprintf(stderr, "QUEUE TYPE: %d\n", bm_type);
	if (bm_type == BM_NONE){

		fprintf(stderr, "No Queue.\n");
		return;
	}else if(bm_type==BM_TO_QUEUE || bm_type==BM_TO_LOCK_FREE_QUEUE){

		init_shards_slabs(epoch_length, 32000, slab_sizes, factor, R_initialize);
	}
	


	fprintf(stderr, "----------------------->GUS: Init Benchmarking\n");
	switch(queue_type) {
		case BM_NONE: {
			fprintf(stderr, "No Queue.\n");
			;
		} break;
		case BM_PRINT: {
			fprintf(stderr, "Print\n");
			;
		} break;
		case BM_DIRECT_FILE: {
			fprintf(stderr, "Direct to File.\n");
			bm_output_fd = open(bm_output_filename, 
							 O_WRONLY | O_CREAT | O_TRUNC,
							 S_IRUSR, S_IWUSR);
		} break;
		case BM_TO_QUEUE: {
			fprintf(stderr, "Message Queue with Locks.\n" );
			bm_oq_init(&bm_oq);
		} break;
		case BM_TO_LOCK_FREE_QUEUE: {
			//fprintf(stderr, "Lock Free Queue. Capacity: %d\n", QUEUE_LENGTH);
			//bm_mpsc_oq = mpscq_create(NULL, QUEUE_LENGTH);
			// {
			//     bm_mpsc_oq = mpscq_create(NULL, QUEUE_LENGTH);
			// }
			// {
			//     size_t buf_len = QUEUE_LENGTH * sizeof(bm_op_t);
			//     buf = malloc(buf_len);
			//     memset(buf, 0, buf_len);
			//     ringbuf_get_sizes(MAX_WORKERS, &ringbuf_obj_size, NULL);
			//     bm_ringbuf = malloc(ringbuf_obj_size);
			//     memset(bm_ringbuf, 0, ringbuf_obj_size);
			//     ringbuf_setup(bm_ringbuf, MAX_WORKERS, buf_len);
			//     w1 = ringbuf_register(bm_ringbuf, 0);
			// }
			{
				// @ Gus: plus one extra for the necessary dummy element
				printf( "Ring Buffer...\n");
				re = malloc( sizeof(struct lfds711_ringbuffer_element) * (QUEUE_LENGTH + 1) );
				lfds711_ringbuffer_init_valid_on_current_logical_core( &rs, re, QUEUE_LENGTH + 1, NULL );
			}

		} break;
		case BM_TO_ZEROMQ: {
			zmq_context = zmq_ctx_new ();
			zmq_sender = zmq_socket (zmq_context, ZMQ_PUB); 
			pthread_mutex_init(&zeroMQ_lock,NULL);
			int zeromq_socket_opt_value = 0;
			int rc =  zmq_setsockopt (zmq_sender, ZMQ_SNDHWM,&zeromq_socket_opt_value , sizeof(int)); 

			rc = zmq_bind(zmq_sender, "tcp://*:5555");
			printf("result of zmq_bind(): %d\n", rc);
			fprintf (stderr, "Started zmq server...\n");
		} break;
	}



	fprintf(stderr, "QUEUE LENGTH: %d\n", QUEUE_LENGTH);
}

static
void bm_write_line_op(int fd, bm_op_t op) {
	size_t str_buffer_length = 3 + 10;
	char* str_buffer = malloc(str_buffer_length);
	sprintf(str_buffer, "%d %"PRIu64" %d \n", op.type, op.key_hv, op.slab_id);
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

	switch(bm_process_op_type) {
		case BM_PROCESS_DUMMY: {
			;
		} break;
		case BM_PROCESS_ADD: {
			random_accum += rand();
			return;
		} break;
		case BM_PROCESS_SPIN: {
			struct timeval t1, t2;
			gettimeofday(&t1, NULL);
			double elapsed = 0;
			do {
				gettimeofday(&t2, NULL);
				elapsed = t2.tv_usec - t1.tv_usec;
			} while(elapsed < SPIN_TIME);
			return;
		} break;
		case BM_PROCESS_PRINT: {
			fprintf(stderr, "type: %d, key: %"PRIu64"\n", op.type, op.key_hv);
			return;
		} break;
	}
	
	struct timespec ti = {0, 1000000};
	// bm_write_line_op(bm_output_fd, op);
	unsigned int slab_ID = 0;
	uint64_t *object = malloc(sizeof(uint64_t));
	*object = op.key_hv;

	//nanosleep(&ti, NULL);
	slab_ID = op.slab_id - 128;
	//printf("## Slab ID: %4"PRIu8"  Slab new ID: %4u  KEY: %11"PRIu64" TYPE: %d NUM_OBJ: %15d\n", op.slab_id, slab_ID, op.key_hv, op.type, number_of_objects);

	int dummy = 0;
	for(int o = 0; o<=10000; o++){
		//this is a dummy function
		if (dummy%2==0){
			dummy++;
		}else{
			dummy--;
		}

	}

	shards_process_object_slab(slab_ID, object);

}

static
void bm_consume_ops(){
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
			//printf("CONSUME_OPS Max Set Size: %u\n", shards->S_max);
			while(NULL != item) {
				bm_process_op(item->op);
				free_oq_item(item);
				item = bm_oq_pop(&bm_oq);
			}
		} break;
		case BM_TO_LOCK_FREE_QUEUE: {
			// {
			//     void* item = mpscq_dequeue(bm_mpsc_oq);
			//     while(NULL != item) {
			//         bm_op_t* op_ptr = item;
			//         bm_process_op(*op_ptr);
			//         free(op_ptr);
			//         item = mpscq_dequeue(bm_mpsc_oq);
			//     }    
			// }
			// {
	  //           size_t len, woff;
	  //           len = ringbuf_consume(bm_ringbuf, &woff);
	  //           size_t n_op = len/sizeof(bm_op_t);
	  //           for (int i = 0; i < n_op; ++i) {
	  //               bm_op_t op;
	  //               memcpy(&op, &buf[woff], sizeof(bm_op_t));
	  //               bm_process_op(op);
	  //               woff += sizeof(bm_op_t);
	  //           }
	  //           ringbuf_release(bm_ringbuf, len);
	  //       }
			{
				void* item;
				int rv = lfds711_ringbuffer_read( &rs, &item, NULL );
				while(1 == rv) {
					bm_op_t* op_ptr = item;
					bm_process_op(*op_ptr);
					free(op_ptr);
					rv = lfds711_ringbuffer_read( &rs, &item, NULL );
			}
			
		} break;
		case BM_TO_ZEROMQ: {
			;
		} break;
	}
	}
}
// @ Gus: LIBEVENT
static
struct event_base* bm_event_base;

static
void bm_clock_handler(evutil_socket_t fd, short what, void* args) {
	// fprintf(stderr, "Tick\n");
	//printf("CLOCK_HANDLER Max Set Size: %u \n", ((SHARDS*)args)->S_max);
	bm_consume_ops();
}

static
void bm_libevent_loop() {
	bm_event_base = event_base_new();
	//printf("%s bm_libevent_loop\n", message);
	//printf("LIBEVENT_LOOP Max Set Size: %u\n", shards->S_max);
	struct event* timer_event = event_new(bm_event_base,
								   -1,
								   EV_TIMEOUT | EV_PERSIST,
								   bm_clock_handler,
								   NULL);
	
	

	/*struct event* timer_event = event_new(bm_event_base,
								   -1,
								   EV_TIMEOUT | EV_PERSIST,
								   bm_clock_handler,
								   shards);
	*/
	struct timeval t = {2, 0};
	event_add(timer_event, &t);

	event_base_dispatch(bm_event_base);
}

void* bm_loop_in_thread(void* args) {
	//SHARDS * shards = (SHARDS*)args;

	if(bm_type==BM_NONE){
		return;
	}
	//fprintf(stderr, "Mensaje de bm_loop_in_thread\n");
	bm_output_fd = open(bm_output_filename, 
						O_WRONLY | O_CREAT | O_TRUNC,
						S_IRUSR | S_IWUSR);
	
	
	//printf("LOOP_IN_THREAD Max Set Size: %u\n", shards->S_max);
	bm_libevent_loop();
	// while(true) bm_consume_ops();
	return NULL;
}

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
			//MPSC implementation
			//bm_mpsc_oq_enqueue(op);
			// {
			//     bm_mpsc_oq_enqueue(op);    
			// }
			
			// {
			//     ssize_t off = ringbuf_acquire(bm_ringbuf, w1, sizeof(bm_op_t));
			//     memcpy(&buf[off], &op, sizeof(bm_op_t));
			//     ringbuf_produce(bm_ringbuf, w1);
			// }
			{
				bm_op_t* op_ptr = malloc(sizeof(bm_op_t));
				*op_ptr = op;
				lfds711_ringbuffer_write( &rs, 
										op_ptr, 
										NULL, 
										&overwrite_occurred_flag, 
										NULL, 
										NULL );
				if( overwrite_occurred_flag == LFDS711_MISC_FLAG_RAISED ) {
					fprintf(stderr, "----------------------->GUS: OVERRIDED ELEMENT IN RING BUFFER\n");
				}
			}

		} break;
		case BM_TO_ZEROMQ: {
			// fprintf(stderr, "sending op: %d, hv: %"PRIu64"\n", op.type, op.key_hv);
			pthread_mutex_lock(&zeroMQ_lock);
			zmq_send(zmq_sender, &op, sizeof(bm_op_t), ZMQ_DONTWAIT);
			pthread_mutex_unlock(&zeroMQ_lock);
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
