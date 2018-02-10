#include "SHARDS.h"
#include "memcached.h"
#include "shards_monitor.h"

static SHARDS *shards_array[MAX_NUMBER_OF_SLAB_CLASSES];
static unsigned int item_sizes[MAX_NUMBER_OF_SLAB_CLASSES];

static int NUMBER_OF_SHARDS =0;
int number_of_objects=0;
int OBJECT_LIMIT= 1000000;
unsigned int epoch =1;
char* mrc_path = "./Results/";
char file_name[40];
FILE *mrc_file;



void initialize_shard_n(const int max_obj,const int i, const double R_initialize, const unsigned int size){
	shards_array[i-1] = SHARDS_fixed_size_init_R(max_obj,R_initialize ,10, Uint64);
	item_sizes[i-1] = size;
}

void init_shards_slabs(const int max_obj,uint32_t *slab_sizes, double factor, double R_initialize){
	int power_largest;
    int i = POWER_SMALLEST - 1;
    unsigned int size = sizeof(item) + settings.chunk_size; 
        //printf("SIZE OF ITEM: %u\n", size);
    while (++i < MAX_NUMBER_OF_SLAB_CLASSES-1) {
        if (slab_sizes != NULL) {
            if (slab_sizes[i-1] == 0)
                break;
            size = slab_sizes[i-1];
        } else if (size >= settings.slab_chunk_size_max / factor) {
            break;
        }
            /* Make sure items are always n-byte aligned */
        if (size % CHUNK_ALIGN_BYTES)
            size += CHUNK_ALIGN_BYTES - (size % CHUNK_ALIGN_BYTES);



            //initializa each SHARDS struct in the shards array. Index is [i-1] because the numbering starts at 
            // one and no at zero.
        initialize_shard_n(max_obj, i, R_initialize, size);

            //fprintf(stderr,"JORGE SIZE %d: %u\n", i, size);
        if (slab_sizes == NULL)
            size *= factor;
    }

    power_largest = i;
    NUMBER_OF_SHARDS=i;
    size = settings.slab_chunk_size_max;
    initialize_shard_n(max_obj, i, R_initialize, size);
        //fprintf(stderr,"JORGE SIZE %d: %u\n", i, size);
    

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    
    for(int j=0; j < NUMBER_OF_SHARDS; j++){
        printf("R Value %2d: %1.3f\n",j+1, shards_array[j]->R);

    }

}

void calculate_miss_rate_curve(){

    //fprintf(stderr, "Number of SHARDS : %2d\n", NUMBER_OF_SHARDS);
    for( int k =0; k< NUMBER_OF_SHARDS; k++){
        //fprintf(stderr, "Number of Objects: %7d\n" , shards_array[k]->num_obj);

        if(shards_array[k]->num_obj !=0 ){
            snprintf(file_name,40,"%sMRC_epoch_%05d_slab_%02d.csv",mrc_path, epoch, k+1);

            //fprintf(stderr, "SHARDS number %2d: %p\n", k+1, shards_array[k]);
            //fprintf(stderr, "Calculating MRC of Slab %2d (size %10u)\n", k+1, item_sizes[k]);

            //fprintf(stderr,"1)-----total_objects : %u\n", shards_array[k]->total_objects); 

            GHashTable *mrc = MRC_empty(shards_array[k]);
            //fprintf(stderr,"2)-----total_objects : %u\n", shards_array[k]->total_objects); 
            //fprintf(stderr, "MRC was created.\n" );
            GList *keys = g_hash_table_get_keys(mrc);
            //fprintf(stderr, "Keys were accessed.\n");
            keys = g_list_sort(keys, (GCompareFunc) intcmp);
            //fprintf(stderr, "\n %d %s \n", k+1, file_name);

            mrc_file = fopen(file_name,"w");

            //printf("WRITING MRC FILE %2d...\n",k+1);
            GList *first = keys;
            while(keys!=NULL){

                    //printf("key: %7d  \n",*(int*)keys->data );
                    //printf("Value: %1.6f\n",*(double*)g_hash_table_lookup(mrc, keys->data) );
                    //printf("key: %7d  Value: %1.6f\n",*(int*)keys->data, *(double*)g_hash_table_lookup(mrc, keys->data) );
                fprintf(mrc_file,"%7d,%1.7f\n",*(int*)keys->data, *(double*)g_hash_table_lookup(mrc, keys->data) );
                //printf("HEY!\n");
                keys=keys->next;
            }


            fclose(mrc_file);
                //printf("MRC FILE WRITTEN! :D\n");

                //printf("R Value:%f\n", shards_array[k]->R);
                //printf("T Value:%"PRIu64"\n", shards_array[k]->T);
            g_list_free(first);
            g_hash_table_destroy(mrc);

        }
    }
}

void shards_process_object_slab(const unsigned int slab_ID, uint64_t *object){
    number_of_objects++;

    SHARDS_feed_obj(shards_array[slab_ID -1] ,object , sizeof(uint64_t));
    
    //printf("Number of objects received: %d\n", number_of_objects );

    //printf("%d\n", (shards2)->num_obj);

    //printf("type: %d, key: %"PRIu64"\n", op.type, op.key_hv);

    if(number_of_objects==OBJECT_LIMIT){
        //printf("CALCULATING Miss Rate Curves...\n");

        calculate_miss_rate_curve();

        number_of_objects = 0;
        epoch++;

    }

}

