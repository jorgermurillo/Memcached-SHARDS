#ifndef __SHARDS_MONITOR_H
#define __SHARDS_MONITOR_H


void shards_process_object_slab(const unsigned int slab_ID, uint64_t *object);
void init_shards_slabs(int max_obj, uint32_t *slab_sizes, double factor, double R_initialize);
#endif