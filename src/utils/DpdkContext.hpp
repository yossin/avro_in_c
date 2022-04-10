#pragma once

#include <rte_eal.h>
#include <rte_hash.h>

#include <string>
#include <iostream>
#include <list>
#include <array>
#include <rte_random.h>
#include <memory>
#include <sstream>


struct dpdk_settings_s{
    const int *cores;
    int channels;
    int socket_mem;
};


 const int def_cores[] ={1,2};
typedef dpdk_settings_s dpdk_settings_t;
dpdk_settings_t default_dpdk_settings = {.cores=def_cores, .channels=4, .socket_mem=2048};


dpdk_settings_t new_dpdk_settings(const int* cores=default_dpdk_settings.cores, 
int channels=default_dpdk_settings.channels, 
int socket_mem=default_dpdk_settings.socket_mem){
    dpdk_settings_t s=
        {.cores=cores, 
            .channels=channels, 
            .socket_mem=socket_mem};
    return s;
}


class DpdkContext final {
    private:     
        DpdkContext(dpdk_settings_t settings) { 
            std::list<int> cores;//(std::begin(settings.cores), std::end(settings.cores));
            cores.unique();
            std::stringstream core_stream;
            for (int c: cores){
                core_stream <<  "," << c;
            }
            std::array<char*,6> x;
        


            x[0]="-l";
            //x[1]=core_stream.str().c_str();
            x[2]="-n";
            //x[3]=std::to_string(settings.channels);
            x[4]="--socket-mem";
            //x[5]=std::to_string(settings.socket_mem);


            std::cout << "initialize dpdk EAL";
            int ret = rte_eal_init(x.size(), x.data());
            std::cout << "rte_eal_init returned: " << ret <<std::endl;
            rte_srand (0);
        }
public:

    ~DpdkContext() {  
        int ret = rte_eal_cleanup();
        std::cout << "rte_eal_cleanup returned: " << ret <<std::endl;
    }

    static DpdkContext& instance(dpdk_settings_s settings=default_dpdk_settings){
        static const std::unique_ptr<DpdkContext> instance{new DpdkContext{settings}};
        return *instance;
    }
};
