#pragma once

#include "mymempool.hpp"
#include <unistd.h>

#define Q(x) #x
#define QUOTE(x) Q(x)

#define MESSAGE_SIZE 500
#define KEY_SIZE 100
#define BATCH_SIZE 16384

typedef std::chrono::time_point<std::chrono::high_resolution_clock> mtime;


typedef struct stats_t{
        unsigned long processed;
        unsigned long processed_bytes;
        unsigned long delivered;
        unsigned long delivered_bytes;
        unsigned long ring_errors;
        unsigned long pool_errors;
} stats_s;

typedef struct mempool<MESSAGE_SIZE,KEY_SIZE> mempool_s;

typedef struct mempool_element<MESSAGE_SIZE,KEY_SIZE> mempool_element_s;


void app_init(mempool_s **pool,stats_s &stats, rte_ring **ring, bool primary){
    *pool = NULL;
    memset(&stats,0, sizeof(stats_s));
    
    // create a ring for transferring data between processes
    *ring = rte_ring_lookup("my_ring");
    if (*ring == NULL && primary){
        *ring = rte_ring_create("my_ring",16384,0, RING_F_MC_HTS_DEQ | RING_F_MP_HTS_ENQ);
        assert (ring!=NULL);
    } else {
        fprintf(stderr, "%% waiting for ring to be created");

        while ((*ring = rte_ring_lookup("my_ring"))==NULL){
            sleep(1);
            fprintf(stderr, ".");
        }
        fprintf(stderr, "\n");
    }

    // share my mempool
    rte_ring *pring = rte_ring_lookup("my_pool_ring");
    if (pring == NULL && primary){
            pring = rte_ring_create("my_pool_ring",1,0, RING_F_MC_HTS_DEQ | RING_F_MP_HTS_ENQ);
            *pool = new mempool_s(1000000);
            rte_ring_enqueue(pring, pool);
    } else {
        fprintf(stderr, "%% waiting for mempool ring to be created");
        while ((pring = rte_ring_lookup("my_pool_ring"))==NULL){
            sleep(1);
            fprintf(stderr, ".");
        }

        fprintf(stderr, "%% waiting for mempool to be allocated");
        while (*pool  == NULL){
            rte_ring_dequeue(pring, (void**)pool);
            sleep(1);
            fprintf(stderr, ".");
        }   
        rte_ring_enqueue(pring, *pool);
        fprintf(stderr, "\n");
    }

}

void app_free(mempool_s **pool, bool primary){
    if (primary){
        rte_ring *pring = rte_ring_lookup("my_pool_ring");
        if (pring){
            fprintf(stderr, "%% releasing  my_pool_ring\n");
            rte_ring_free(pring);
        }

        rte_ring *ring = rte_ring_lookup("my_ring");
        if (ring){
            fprintf(stderr, "%% releasing  my_ring\n");
            rte_ring_free(ring);
        }

        delete *pool;
        *pool=NULL;
    }
}

inline void print_stat(const mtime &start_time, const char* action, stats_s &stats){
    if ((stats.processed % 1000000)>0){
            return;
    }
    auto end_time = std::chrono::high_resolution_clock::now();
    auto elapsed = end_time - start_time;
    long time_spent = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
    double message_delivery_rate=((double)stats.delivered)/((double)time_spent);
    double bytes_delivery_rate=((double)stats.delivered_bytes)/((double)time_spent)/1000000.0;

    double message_process_rate=((double)stats.processed)/((double)time_spent);
    double bytes_process_rate=((double)stats.processed_bytes)/((double)time_spent)/1000000.0;

    fprintf(stderr, "%s: %lusec elapsed. \n  %ld processed: %.1fmsg/s %.1fmb/s\n  %lu delivered: %.1fmsg/s %.1fmb/s\n  pool errors %lu, ring errors %lu\n",
                        action, time_spent, 
                        stats.processed, message_process_rate, bytes_process_rate,
                        stats.delivered, message_delivery_rate, bytes_delivery_rate,
                        stats.pool_errors, stats.ring_errors);
}
