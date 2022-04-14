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


#define Q(x) #x
#define QUOTE(x) Q(x)

#define MESSAGE_SIZE 500
#define KEY_SIZE 100
//10000, 20000, 163840
#define BATCH_SIZE 16384
#define RD_MAX_BUFFERED_MESSAGES 100000
#define RD_MAX_BUFFERED_MS 100



/* Typical include path would be <librdkafka/rdkafka.h>, but this program
 * is builtin from within the librdkafka source tree and thus differs. */
#include "librdkafka/rdkafka.h"


static volatile sig_atomic_t run = 1;
static auto start_time = std::chrono::high_resolution_clock::now();

/**
 * @brief Signal termination of program
 */
static void stop(int sig) {
        run = 0;
        fclose(stdin); /* abort fgets() */
}

inline void print_stat(const char* action, unsigned long num){
        if (num % BATCH_SIZE*2){
                return;
        }
        auto end_time = std::chrono::high_resolution_clock::now();
        auto elapsed = end_time - start_time;
        long time_spent = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
        double message_rate=((double)num)/((double)time_spent);
        double bytes_rate=message_rate*MESSAGE_SIZE/1000000;

        fprintf(stderr, "%% %s %lu messages, within %ld sec. msg/s %f mb/s %f\n",
                        action, num, time_spent, message_rate, bytes_rate);
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
        static unsigned long delivered=0;
        static unsigned long last_key=0;
        static unsigned long got_non_bigger=0;
        static unsigned long errors=0;
        if (rkmessage->err){
                fprintf(stderr, "%% Message delivery failed: %s\n",
                        rd_kafka_err2str(rkmessage->err));
                errors++;
        } else {
                delivered++;
                unsigned long key = *((long*)rkmessage->key);
                if (! (key>last_key) ){
                       got_non_bigger++; 
                }
                last_key=key;
                print_stat("DELIVERED", delivered);
                // if (key%(BATCH_SIZE*10) ==0){
                        // fprintf(stderr,
                        //   "%% Last delivered message (%zd bytes, partition %d, key %lu. totals: deliverd=%lu, errors=%u\n",
                        //   rkmessage->len, rkmessage->partition,key, delivered, errors);
                // }

        }

        mempool<MESSAGE_SIZE,KEY_SIZE>* pool = (mempool<MESSAGE_SIZE,KEY_SIZE>*)opaque;
        struct mempool_element<MESSAGE_SIZE,KEY_SIZE>* mbuffer = (struct mempool_element<MESSAGE_SIZE,KEY_SIZE>*)rkmessage->_private;
        assert(pool!=NULL);
        assert(mbuffer!=NULL);
        pool->put(mbuffer);
}

void prepare_batch(rd_kafka_t *rk, mempool<MESSAGE_SIZE,KEY_SIZE>* pool, rd_kafka_message_t *batch, long &key, size_t accepted){
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
                struct mempool_element<MESSAGE_SIZE,KEY_SIZE>* mbuffer = NULL;
                while(true) {
                        mbuffer=pool->get();
                        if (mbuffer){
                                break;
                        } else {
                                rd_kafka_poll(rk, 0);
                        }
                }

                print_stat("SENT", key);

                void* buff = mbuffer->buffer;
                long* k= (long*)buff;
                *k=++key;
                batch[i].payload=buff;
                batch[i].len=MESSAGE_SIZE;
                batch[i].key=k;
                batch[i].key_len=sizeof(long);
                batch[i]._private=mbuffer;                    
        }
        rd_kafka_poll(rk, 0);


}


int main(int argc, char **argv) {
        rd_kafka_t *rk;        /* Producer instance handle */
        rd_kafka_conf_t *conf; /* Temporary configuration object */
        char errstr[512];      /* librdkafka API error reporting buffer */
        char buf[512];         /* Message value temporary buffer */
        const char *brokers;   /* Argument: broker list */
        const char *topic_prefix;     /* Argument: topic to produce to */
        int topic_num;     /* Argument: topic to produce to */
        long key=0;
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

        mempool<MESSAGE_SIZE,KEY_SIZE>* pool= new mempool<MESSAGE_SIZE,KEY_SIZE>(topic_num*BATCH_SIZE);

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
        
        start_time = std::chrono::high_resolution_clock::now();


        while (run && key<limit) {

                for (int i=0;i<topic_num;i++){
                        prepare_batch(rk, pool, buffers[i],key, accepted_messages[i]);
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
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto elapsed = end_time - start_time;
        long time_spent = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
        fprintf(stderr, "%% Sent %lu messages, within %ld sec. msg/sec %f\n",
                        key, time_spent, ((double)key)/((double)time_spent));
        


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
        delete pool;
        return 0;
}

