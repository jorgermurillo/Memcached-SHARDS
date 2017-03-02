typedef enum {
	BM_NONE,
	BM_PRINT,
	BM_DIRECT_FILE,
	BM_TO_QUEUE,
} bm_type_t;

typedef enum {
    BM_READ_OP,
    BM_WRITE_OP,
} bm_op_type_t;

typedef struct {
    bm_op_type_t type;
    char*      	 key;
    size_t	     key_length;
} bm_op_t;

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
	free(oq_item->op.key);
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
bm_type_t bm_type = BM_TO_QUEUE;

char bm_output_filename[] = "benchmarking_output.txt";
int  bm_output_fd = -1;

bm_oq_t bm_oq;

// @ Gus: bm functions
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
    						 O_WRONLY | O_CREAT,
    						 S_IRUSR);
    	} break;
    	case BM_TO_QUEUE: {
    		bm_oq_init(&bm_oq);
    	} break;
    }
}

static
void bm_write_line_op_no_free_key(int fd, bm_op_t op) {
	size_t str_buffer_length = 3 + op.key_length;
	char* str_buffer = malloc(str_buffer_length);
	sprintf(str_buffer, "%d %s\n", op.type, op.key);
	write(fd, str_buffer, str_buffer_length);
	free(str_buffer);
}

static
void bm_write_line_op(int fd, bm_op_t op) {
	bm_write_line_op_no_free_key(fd, op);
	free(op.key);
}

static
void bm_write_op_to_oq(bm_oq_t* oq, bm_op_t op) {
	bm_oq_item_t* item = malloc_oq_item();
	item->op = op;
	bm_oq_push(oq, item);
}

// @ Gus: LIBEVENT
static
struct event_base* bm_event_base;

static
void bm_clock_handler(evutil_socket_t fd, short what, void* args) {
	fprintf(stderr, "Tick\n");
	bm_oq_item_t* item = bm_oq_pop(&bm_oq);
	while(NULL != item) {
		fprintf(stderr, "op: %d key: %s\n", item->op.type, item->op.key);
		bm_write_line_op_no_free_key(bm_output_fd, item->op);
		free_oq_item(item);
		item = bm_oq_pop(&bm_oq);
	}
}

static
void bm_loop() {
	bm_event_base = event_base_new();

	struct event* timer_event = event_new(bm_event_base,
								   -1,
								   EV_TIMEOUT | EV_PERSIST,
								   bm_clock_handler,
								   NULL);
	struct timeval t = {2, 0};
    event_add(timer_event, &t);

    bm_output_fd = open(bm_output_filename, 
						O_WRONLY | O_CREAT,
						S_IRUSR);
	event_base_dispatch(bm_event_base);
}

static
void* bm_loop_in_thread(void* args) {
	bm_loop();
	return NULL;
}

static
void bm_record_read_op(char* key, size_t key_length) {
	char* new_key = malloc(key_length);
	strcpy(new_key, key);
    bm_op_t op = {BM_READ_OP, new_key, key_length};
    switch(bm_type) {
    	case BM_NONE: {
    		;
    	} break;
    	case BM_PRINT: {
    		fprintf(stderr, "----------------------->GUS: PROCESS GET COMMANDD WITH KEY: %s\n", op.key);
    	} break;
    	case BM_DIRECT_FILE: {
 			bm_write_line_op(bm_output_fd, op);
    	} break;
    	case BM_TO_QUEUE: {
    		bm_write_op_to_oq(&bm_oq, op);
    	} break;
    }
}

static
void bm_record_write_op(char* command, char* key, size_t key_length) {
	char* new_key = malloc(key_length);
	strcpy(new_key, key);
	bm_op_t op = {BM_WRITE_OP, new_key, key_length};
	switch(bm_type) {
		case BM_NONE: {
    		;
    	} break;
    	case BM_PRINT: {
    		fprintf(stderr, "----------------------->GUS: PROCESS %s COMMANDD WITH KEY: %s\n", command, op.key);
    	} break;
    	case BM_DIRECT_FILE: {
    		bm_write_line_op(bm_output_fd, op);
    	} break;
    	case BM_TO_QUEUE: {
    		bm_write_op_to_oq(&bm_oq, op);
    	} break;
    }
}