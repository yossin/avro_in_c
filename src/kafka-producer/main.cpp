#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <list>
#include <queue>
#include <unistd.h>
#include "mymempool.hpp"
#include <chrono>
#include <limits.h>
#include "common.hpp"
#include "DpdkContext.hpp"

#define RD_MAX_BUFFERED_MESSAGES 100000
#define RD_MAX_BUFFERED_MS 100



/* Typical include path would be <librdkafka/rdkafka.h>, but this program
 * is builtin from within the librdkafka source tree and thus differs. */
#include "librdkafka/rdkafka.h"


static volatile sig_atomic_t run = 1;
static mtime global_start_time;

/**
 * @brief Signal termination of program
 */
static void stop(int sig) {
        run = 0;
        fclose(stdin); /* abort fgets() */
}




/**
 * @brief Message delivery report callback.
 *
 * This callback is called exactly once per message, indicating if
 * the message was succesfully delivered
 * (rkmessage->err == RD_KAFKA_RESP_ERR_NO_ERROR) or permanently
 * failed delivery (rkmessage->err != RD_KAFKA_RESP_ERR_NO_ERROR).
 *
 * The callback is triggered from rd_kafka_poll() and executes on
 * the application's thread.
 */
static void
dr_msg_cb(rd_kafka_t *rk, const rd_kafka_message_t *rkmessage, void *opaque) {
        static stats_t stats ={0};

        if (rkmessage->err){
                fprintf(stderr, "%% Message delivery failed: %s\n",
                        rd_kafka_err2str(rkmessage->err));
                stats.kafka_errors++;
        } else {
                stats.delivered++;
                stats.delivered_bytes+=rkmessage->len;
        }
        stats.processed++;
        stats.processed_bytes+=rkmessage->len;

        
        print_stat(global_start_time, "DELIVERED", stats);


        mempool_s *pool = (mempool_s*)opaque;
        mempool_element_s * mbuffer = (mempool_element_s*)rkmessage->_private;
        assert(pool!=NULL);
        assert(mbuffer!=NULL);
        pool->put(mbuffer);
}

void prepare_batch(rd_kafka_t *rk, rte_ring *ring, stats_s &stats, rd_kafka_message_t *batch, size_t accepted){
        if (accepted ==0){
                return;
        }

        for (int i=0,j=accepted;j<BATCH_SIZE;i++,j++){
                batch[i].payload=batch[j].payload;
                batch[i].len=batch[j].len;
                batch[i].key=batch[j].key;
                batch[i].key_len=batch[j].key_len;
                batch[i]._private=batch[j]._private;
        }

        for (int i=BATCH_SIZE-accepted;i<BATCH_SIZE;i++){
                mempool_element_s *mbuffer = NULL;
                rte_ring_dequeue(ring, (void**)&mbuffer);
                while(mbuffer==NULL) {
                        stats.ring_errors++;
                        rte_ring_dequeue(ring, (void**)&mbuffer);
                        rd_kafka_poll(rk, 0);       
                }
                

                print_stat(global_start_time, "KAFKA", stats);

                batch[i].payload=mbuffer->buffer;
                batch[i].len=mbuffer->buffer_len;
                batch[i].key=mbuffer->key;
                batch[i].key_len=mbuffer->key_len;
                batch[i]._private=mbuffer;        
                stats.processed++;
                stats.processed_bytes+=mbuffer->buffer_len;    
        }
        rd_kafka_poll(rk, 0);


}


int main(int argc, char **argv) {
        rd_kafka_t *rk;        /* Producer instance handle */
        rd_kafka_conf_t *conf; /* Temporary configuration object */
        char errstr[512];      /* librdkafka API error reporting buffer */
        //char buf[512];         /* Message value temporary buffer */
        const char *brokers;   /* Argument: broker list */
        const char *topic_prefix;     /* Argument: topic to produce to */
        int topic_num;     /* Argument: topic to produce to */
        unsigned long key=0;
        unsigned long limit=0;


        /*
         * Argument validation
         */
        if (argc < 4 || argc >5) {
                fprintf(stderr, "%% Usage: %s <broker> <topic_prefix> <topic_number> [limit]\n", argv[0]);
                return 1;
        }

        brokers = argv[1];
        topic_prefix   = argv[2];
        topic_num   = strtol(argv[3], NULL,10);
        limit   = (argc == 5) ? strtoul (argv[4], NULL,10) : ULONG_MAX;

        dpdk_settings_t settings=default_dpdk_settings;
        settings.cores={1};
        settings.primary=false;
        auto c = DpdkContext::instance(settings);

        mempool_s *pool=NULL;
        stats_s stats;
        rte_ring *ring=NULL;

        app_init(&pool, stats, &ring, c.dpdk_settings.primary);

        /*
         * Create Kafka client configuration place-holder
         */
        conf = rd_kafka_conf_new();

        /* Set bootstrap broker(s) as a comma-separated list of
         * host or host:port (default port 9092).
         * librdkafka will use the bootstrap brokers to acquire the full
         * set of brokers from the cluster. */
        if (rd_kafka_conf_set(conf, "bootstrap.servers", brokers, errstr,
                              sizeof(errstr)) != RD_KAFKA_CONF_OK) {
                fprintf(stderr, "%s\n", errstr);
                return 1;
        }
        if (rd_kafka_conf_set(conf, "queue.buffering.max.ms", QUOTE(RD_MAX_BUFFERED_MS), errstr,
                              sizeof(errstr)) != RD_KAFKA_CONF_OK) {
                fprintf(stderr, "%s\n", errstr);
                return 1;
        } 
        if (rd_kafka_conf_set(conf, "queue.buffering.max.messages", QUOTE(RD_MAX_BUFFERED_MESSAGES), errstr,
                              sizeof(errstr)) != RD_KAFKA_CONF_OK) {
                fprintf(stderr, "%s\n", errstr);
                return 1;
        }
        if (rd_kafka_conf_set(conf, "batch.num.messages", QUOTE(BATCH_SIZE), errstr,
                              sizeof(errstr)) != RD_KAFKA_CONF_OK) {
                fprintf(stderr, "%s\n", errstr);
                return 1;
        }

        


        /* Set the delivery report callback.
         * This callback will be called once per message to inform
         * the application if delivery succeeded or failed.
         * See dr_msg_cb() above.
         * The callback is only triggered from rd_kafka_poll() and
         * rd_kafka_flush(). */
        rd_kafka_conf_set_dr_msg_cb(conf, dr_msg_cb);

        rd_kafka_conf_set_opaque(conf, pool);

        /*
         * Create producer instance.
         *
         * NOTE: rd_kafka_new() takes ownership of the conf object
         *       and the application must not reference it again after
         *       this call.
         */
        rk = rd_kafka_new(RD_KAFKA_PRODUCER, conf, errstr, sizeof(errstr));
        if (!rk) {
                fprintf(stderr, "%% Failed to create new producer: %s\n",
                        errstr);
                return 1;
        }
        rd_kafka_topic_t **ktarr = new rd_kafka_topic_t*[topic_num];
        for (int i=0;i<topic_num;i++){
                char topic[500];
                sprintf(topic, "%s_%d", topic_prefix,i+1);
                ktarr[i]=rd_kafka_topic_new(rk,topic, NULL);
        }

        
        /* Signal handler for clean shutdown */
        signal(SIGINT, stop);

        fprintf(stderr,
                "%% Press Ctrl-C or Ctrl-D to exit\n");


        size_t accepted_messages[topic_num];
        for (int i=0; i<topic_num; i++){
                accepted_messages[i]=BATCH_SIZE;
        }
        
        rd_kafka_message_t buffers[topic_num][BATCH_SIZE];
        
        mtime start_time = std::chrono::high_resolution_clock::now();
        global_start_time=start_time;

        while (run && key<limit) {

                for (int i=0;i<topic_num;i++){
                        prepare_batch(rk, ring, stats, buffers[i], accepted_messages[i]);
                }

                for (int i=0;i<topic_num;i++){
                        accepted_messages[i]=rd_kafka_produce_batch(ktarr[i],0,0,buffers[i],BATCH_SIZE);
                        rd_kafka_poll(rk, 0);
                        if (accepted_messages[i] <BATCH_SIZE){
                                rd_kafka_poll(rk, 0);
                        }
                }
        }



        /* Wait for final messages to be delivered or fail.
         * rd_kafka_flush() is an abstraction over rd_kafka_poll() which
         * waits for all messages to be delivered. */
        fprintf(stderr, "%% Flushing final messages..\n");
        rd_kafka_flush(rk, 10 * 1000 /* wait for max 10 seconds */);
        
        print_stat(start_time, "KAFKA", stats);

        /* If the output queue is still not empty there is an issue
         * with producing messages to the clusters. */
        if (rd_kafka_outq_len(rk) > 0)
                fprintf(stderr, "%% %d message(s) were not delivered\n",
                        rd_kafka_outq_len(rk));

        for (int i=0;i<topic_num; i++){
                rd_kafka_topic_destroy(ktarr[i]);
        }
        delete ktarr;
        /* Destroy the producer instance */
        rd_kafka_destroy(rk);
        app_free(&pool, c.dpdk_settings.primary);
        return 0;
}

