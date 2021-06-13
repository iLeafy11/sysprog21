#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>


#define K 1024
#define MEM_WIDTH 32

struct cache_block {
    int tag;
    bool valid;
};

typedef struct cache_set {
    struct cache_block *blocks;
    int head;
    int top;
} cache_set;

typedef struct cache_block_addr {
    int tag;
    int index;
    // int offset;
    int (*task)(struct cache_block_addr, cache_set *,int);
} block_addr;


static int pow2(int num) {
    int pow = 0;
    for (; num >> 1; num >>= 1) {
        pow++;
    }
    return pow;
}

static void swap(int *a, int *b) {
    int tmp = *a;
    *a = *b;
    *b = tmp;
}

static int FIFO(block_addr addr, cache_set *map, int entry) {
    int ptr = map[addr.index].head;
    int ret_val = map[addr.index].blocks[ptr].tag;
    map[addr.index].blocks[ptr].tag = addr.tag;
    map[addr.index].head = (map[addr.index].head + 1) & (entry - 1);

    return ret_val ? ret_val : -1;
}

static int LRU(block_addr addr, cache_set *map, int entry) {
    int ptr = map[addr.index].top;
    int ret_val = map[0].blocks[ptr].tag;
    for (int i = 0; i < entry - 1; i++) {
        swap(&map[addr.index].blocks[i].tag, &map[addr.index].blocks[i + 1].tag);
    }

    map[addr.index].blocks[entry - 1].tag = addr.tag;
    return ret_val;
}

static int shump(block_addr addr, cache_set *map, int entry) {
    int mush = rand() % entry;
    int ret_val = map[addr.index].blocks[mush].tag;
    map[addr.index].blocks[mush].tag = addr.tag;
    return ret_val;
}

static cache_set *map_alloc(int m, int n) {
    cache_set *map = malloc(m * sizeof(cache_set));
    for (int i = 0; i < m; i++) {
        map[i].blocks = malloc(n * sizeof(struct cache_block));
        memset(map[i].blocks, 0, n);
    }
    return map;
}

static void map_free(cache_set *map, int m) {
    for (int i = 0; i < m; i++) {
        free(map[i].blocks);
    }
    free(map);
}

static int hit_or_miss(cache_set *map, block_addr addr, int entry) {
    int ret_val;
    int i;
    for (i = 0; i < entry; i++) {
        if (!map[addr.index].blocks[i].valid) {
            map[addr.index].blocks[i].valid = true;
            map[addr.index].blocks[i].tag = addr.tag;
            // FIFO
            map[addr.index].head = (map[addr.index].head + 1) & (entry - 1);
            // LRU
            map[addr.index].top = map[addr.index].top < (entry - 1) ? (map[addr.index].top + 1) : (entry - 1);
            return -1;
        } 

        if (map[addr.index].blocks[i].tag == addr.tag) {
            // LRU
            int ptr = map[addr.index].top;
            if (i != ptr) {
                swap(&map[addr.index].blocks[i].tag, &map[addr.index].blocks[ptr].tag);
            }
            return -1;
        } 
    }

    if (i == 1) {
        ret_val = map[addr.index].blocks[0].tag;
        map[addr.index].blocks[0].tag = addr.tag;
        return ret_val;
    }
        
    ret_val = addr.task(addr, map, entry);

    return ret_val;
}

int main(int argc, char *argv[])
{   
    srand(time(NULL));

    FILE * fin = fopen(argv[1], "r+");
    FILE * fout = fopen(argv[2], "w+");

    int cache_size, block_size, associativity, policy;
    fscanf(fin, "%d%d%d%d", &cache_size, &block_size, &associativity, &policy);
    
    int block_num = cache_size * K / block_size;
    int offset_cap = pow2(block_size);
    int index_cap = pow2(block_num);
    int tag_cap = MEM_WIDTH - offset_cap - index_cap;
    
    int set, entry;
    switch (associativity) {
        case 0: // direct map
            set = block_num;
            entry = 1;
            break;
        case 1: // 4-way
            set = block_num >> 2;
            entry = 4;
            index_cap -= 2;
            tag_cap += 2;
            break;
        default: // fully associative
            set = 1;
            entry = block_num;
            index_cap = 0;
            tag_cap += index_cap;
            break;
    }
    
    cache_set *map = map_alloc(set, entry);
    
    int (*call_back)(block_addr, cache_set *, int);
    switch (policy) {
        case 0: // FIFO
            call_back = &FIFO;
            break;
        case 1: // LRU
            call_back = &LRU;
            break;
        default: // random
            call_back = &shump;
            break;
    }

    
    unsigned int addr_in;
    while(fscanf(fin, "%x", &addr_in) != EOF) {
        // printf("%u %u\n", addr_in, 0xbfa437cc);
        block_addr addr = {
            .tag = addr_in >> (offset_cap + index_cap),
            .index = (addr_in >> offset_cap) & ((1 << index_cap) - 1), // module 2^(index_cap)
            // .offset = addr_in << (tag_cap + index_cap),
            .task = call_back
        };
        // printf("%u %u\n", addr.tag, addr.index);
        unsigned int victim;
        victim = hit_or_miss(map, addr, entry);
        
        // if (victim != -1) {
            // printf("%u %u\n", addr_in >> offset_cap >> index_cap, ((unsigned int)(addr_in << tag_cap)) >> tag_cap >> offset_cap);
            // printf("%d\n", victim);
        // }

        fprintf(fout, "%d\n", victim);
        // break;
    }

    map_free(map, set);
    fclose(fin);
    fclose(fout);
    // printf("%d\n", 0xbfa437c8);
    return 0;
}
