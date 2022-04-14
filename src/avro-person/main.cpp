#include <stdio.h>
#include <stdlib.h>
#include <avro.h>
#include <assert.h>
#include <chrono>
#include <signal.h>
#include <limits.h>
#include "DpdkContext.hpp"
#include "mymempool.hpp"
#include <rte_ring.h>
#include "common.hpp"

#define RD_MAX_BUFFERED_MESSAGES 100000
#define RD_MAX_BUFFERED_MS 100


static volatile sig_atomic_t run = 1;
static unsigned long max_messages=ULONG_MAX;



/**
 * @brief Signal termination of program
 */
static void stop(int sig) {
        run = 0;
        fclose(stdin); /* abort fgets() */
}

/* Parse schema into a schema data structure */
void init_schema(avro_schema_t &person_schema) {
        person_schema=avro_schema_string();
        /* A simple schema for our tutorial */
        static const char  PERSON_SCHEMA[] =
                "{\"type\":\"record\",\
                \"name\":\"Person\",\
                \"fields\":[\
                {\"name\": \"ID\", \"type\": \"long\"},\
                {\"name\": \"First\", \"type\": \"string\"},\
                {\"name\": \"Last\", \"type\": \"string\"},\
                {\"name\": \"Phone\", \"type\": \"string\"},\
                {\"name\": \"Age\", \"type\": \"int\"}]}";
        assert (avro_schema_from_json_literal(PERSON_SCHEMA, &person_schema) ==0);
}


/* Create a value to match the person schema and save it */
int add_person(avro_writer_t &writer, avro_value_iface_t  *clazz, int64_t id, 
        const char *first, const char *last, const char *phone, int32_t age) {

        avro_value_t  person;
        avro_generic_value_new(clazz, &person);
        avro_value_t field;

        avro_value_get_by_name(&person, "ID", &field, NULL);
        avro_value_set_long(&field, id);

        avro_value_get_by_name(&person, "First", &field, NULL);
        avro_value_set_string(&field, first);
        
        avro_value_get_by_name(&person, "Last", &field, NULL);
        avro_value_set_string(&field, last);
        
        avro_value_get_by_name(&person, "Age", &field, NULL);
        avro_value_set_int(&field, age);
        
        avro_value_get_by_name(&person, "Phone", &field, NULL);
        avro_value_set_string(&field, phone);
        

        size_t size=0;
        assert(avro_value_sizeof(&person, &size) ==0);
        if (size > MESSAGE_SIZE){
                return -size;
        }
        int res = avro_value_write(writer, &person);

        /* Decrement all our references to prevent memory from leaking */
        avro_value_decref(&person);

        if (res != 0){
                return -abs(res);
        } else {
                return (int) size;
        }
}


/* Create a value to match the person schema and save it */
int add_person_with_wrapped_values(avro_writer_t &writer, avro_value_iface_t  *clazz, int64_t id, 
        const char *first, const char *last, const char *phone, int32_t age) {

        
        avro_value_t  person;
        avro_generic_value_new(clazz, &person);

        avro_value_t field;
        avro_wrapped_buffer_t  wfield;


        avro_value_get_by_name(&person, "ID", &field, NULL);
        avro_value_set_long(&field, id);
        
        avro_value_get_by_name(&person, "First", &field, NULL);
        avro_wrapped_buffer_new_string(&wfield, first);
        avro_value_give_string_len(&field, &wfield);
        
        avro_value_get_by_name(&person, "Last", &field, NULL);
        avro_wrapped_buffer_new_string(&wfield, last);
        avro_value_give_string_len(&field, &wfield);
        
        avro_value_get_by_name(&person, "Age", &field, NULL);
        avro_value_set_int(&field, age);
        
        avro_value_get_by_name(&person, "Phone", &field, NULL);
        avro_wrapped_buffer_new_string(&wfield, phone);
        avro_value_give_string_len(&field, &wfield);
        

        size_t size=0;
        assert(avro_value_sizeof(&person, &size) ==0);
        if (size > MESSAGE_SIZE){
                return -size;
        }
        int res = avro_value_write(writer, &person);

        /* Decrement all our references to prevent memory from leaking */
        avro_value_decref(&person);

        if (res != 0){
                return -abs(res);
        } else {
                return (int) size;
        }
}



int generic_loop(const mtime &start_time, mempool_s *pool, rte_ring* ring, int (*add)(avro_writer_t&, avro_value_iface_t*, int64_t, const char *, const char *, const char *, int32_t),
        avro_value_iface_t  *person_class, int64_t &id, stats_s &stats, 
        const char *first, const char *last, const char *phone, int32_t &age){
        
        
        int last_size=0;
        while(run && stats.delivered<max_messages) {
                // get a pool element and serialize avro mesage into it
                mempool_element_s *elem= pool->get();
                while (elem==NULL){
                        elem= pool->get();
                        stats.pool_errors++;
                }
                avro_writer_t writer = avro_writer_memory(elem->buffer, MESSAGE_SIZE);
                last_size=elem->buffer_len = add(writer, person_class, id, first, last, phone, age);
                memcpy(elem->key, &id, sizeof(id));
                elem->key_len=sizeof(id);
                avro_writer_flush(writer);
                avro_writer_free(writer);
                id++;
                stats.processed++;
                stats.processed_bytes+=last_size;
                // enqueue, on failure put back the element into the pool
                if (rte_ring_enqueue(ring, elem)){ 
                        pool->put(elem);
                        stats.ring_errors++;
                } else {
                        stats.delivered++;
                        stats.delivered_bytes+=last_size;

                }
                print_stat(start_time, "AVRO", stats);
        }
        return last_size;

}



int main(int argc, char **argv) {
        const auto c = DpdkContext::instance();
        
        mempool_s *pool = NULL;
        stats_s stats;
        rte_ring *ring = NULL;

        app_init(&pool, stats, &ring, c.dpdk_settings.primary);




        int64_t id = 0;
        avro_schema_t person_schema;
        /* Signal handler for clean shutdown */
        signal(SIGINT, stop);

        max_messages=(argc==2)?strtoul(argv[1],NULL, 10):ULONG_MAX;
        

        init_schema(person_schema);
        avro_value_iface_t  *person_class = avro_generic_class_from_schema(person_schema);

        const char *first="first name blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa";
        const char *last="last name blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa";
        const char *phone="1234567890";
        int32_t age=100;

        fprintf(stderr, "%% Press Ctrl-C or Ctrl-D to exit \nor wait till %lu messages will be delivered\n", max_messages);

        auto start_time = std::chrono::high_resolution_clock::now();

        // use: add_person OR add_person_with_wrapped_values
        generic_loop(start_time, pool, ring, add_person_with_wrapped_values, 
                person_class, id, stats, first, last, phone, age);

        print_stat(start_time, "AVRO", stats);

        /* Decrement all our references to prevent memory from leaking */
	avro_schema_decref(person_schema);
        avro_value_iface_decref(person_class);

        app_free(&pool, c.dpdk_settings.primary);
}