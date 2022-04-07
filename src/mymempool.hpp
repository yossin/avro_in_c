#include <memory>
#include <stdlib.h>
#include <atomic>
#include <assert.h>

#pragma once

template< std::size_t N >
struct mempool_buff{
    char buffer[N];    
};

template< std::size_t N >
class mempool{
  private:
    unsigned size;
    struct mempool_buff<N>** arr;
    std::list<struct mempool_buff<N>*> refs;
    unsigned long next_read=0;
    unsigned long read_barrier;
    unsigned long last_write;

    
    mempool(const mempool &);
    mempool& operator=(const mempool &);
  public:
    mempool(unsigned pool_size=102400):size(pool_size){
      arr = new struct mempool_buff<N>*[size];
      assert(arr);
      for (int i=0;i<size;i++){
        arr[i] = new struct mempool_buff<N>();
        refs.push_back(arr[i]);
      }
      read_barrier=size;
      last_write=size-1;
    }
    ~mempool(){
      for (struct mempool_buff<N>* ref : refs){
        delete ref;
      }
      delete arr;
    }
    struct mempool_buff<N>* get(){
      unsigned long my_pos=next_read;
      if (next_read < read_barrier){
          if (__sync_bool_compare_and_swap(&next_read, my_pos,my_pos+1)){
              return arr[my_pos%size];
          }
      }
      return NULL;
    }

    void put(struct mempool_buff<N>* buff){
      unsigned long my_pos;
      do{
        my_pos=last_write+1;
      } while (!__sync_bool_compare_and_swap(&last_write, my_pos-1,my_pos));
      assert(my_pos-next_read < size);

      arr[my_pos%size]=buff;
      // must wait for parallel writers to finish before me..
      while (!__sync_bool_compare_and_swap(&read_barrier, my_pos,my_pos+1));
    }

};