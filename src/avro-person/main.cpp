#include <stdio.h>
#include <stdlib.h>
#include <avro.h>
#include <assert.h>
#include <chrono>
#include <signal.h>
#include <limits.h>
#include "DpdkContext.hpp"

#define Q(x) #x
#define QUOTE(x) Q(x)

#define MESSAGE_SIZE 500
//10000, 20000, 163840
#define BATCH_SIZE 16384
#define RD_MAX_BUFFERED_MESSAGES 100000
#define RD_MAX_BUFFERED_MS 100

#define BUFFER_SIZE 1024

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
int add_person(avro_writer_t &writer, avro_value_iface_t  *clazz, int64_t &id, 
        const char *first, const char *last, const char *phone, int32_t age) {

        avro_value_t  person;
        avro_generic_value_new(clazz, &person);
        avro_value_t field;

        avro_value_get_by_name(&person, "ID", &field, NULL);
        avro_value_set_long(&field, ++id);

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
        if (size > BUFFER_SIZE){
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
int add_person_with_wrapped_values(avro_writer_t &writer, avro_value_iface_t  *clazz, int64_t &id, 
        const char *first, const char *last, const char *phone, int32_t age) {

        
        avro_value_t  person;
        avro_generic_value_new(clazz, &person);

        avro_value_t field;
        avro_wrapped_buffer_t  wfield;


        avro_value_get_by_name(&person, "ID", &field, NULL);
        avro_value_set_long(&field, ++id);
        
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
        if (size > BUFFER_SIZE){
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



int generic_loop(int (*add)(avro_writer_t&, avro_value_iface_t*, int64_t&, const char *, const char *, const char *, int32_t),
        avro_value_iface_t  *person_class, int64_t &id, unsigned long &num, 
        const char *first, const char *last, const char *phone, int32_t &age){
        
        char buffer[BUFFER_SIZE];
        int size=0; 
        while(run && num<max_messages) {
                avro_writer_t writer = avro_writer_memory(buffer, BUFFER_SIZE);
                size= add(writer, person_class, id, first, last, phone, age);
                avro_writer_flush(writer);
                avro_writer_free(writer);
                num++;
        }
        return size;

}



int main(int argc, char **argv) {
        const auto c = DpdkContext::instance();

        int64_t id = 0;
        avro_schema_t person_schema;
        /* Signal handler for clean shutdown */
        signal(SIGINT, stop);

        if (argc==2){
                max_messages=strtoul(argv[1],NULL, 10);
        }

        


        init_schema(person_schema);
        avro_value_iface_t  *person_class = avro_generic_class_from_schema(person_schema);

        const char *first="first name blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa";
        const char *last="last name blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa blaa";
        const char *phone="1234567890";
        int32_t age=100;
        int size=0;

        fprintf(stderr, "%% Press Ctrl-C or Ctrl-D to exit\n");

        auto start_time = std::chrono::high_resolution_clock::now();

        unsigned long num=0;
        // use: add_person OR add_person_with_wrapped_values
        size=generic_loop(add_person_with_wrapped_values, 
                person_class, id, num, first, last, phone, age);

        auto end_time = std::chrono::high_resolution_clock::now();
        auto elapsed = end_time - start_time;
        long time_spent = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
        fprintf(stderr, "wrote  %lu essages in %ld sec. msg/sec %f\n",
                         num, time_spent, ((double)num)/((double)time_spent));

        fprintf(stderr, "wrote person. its size is %d\n", size);

        /* Decrement all our references to prevent memory from leaking */
	avro_schema_decref(person_schema);
        avro_value_iface_decref(person_class);


}