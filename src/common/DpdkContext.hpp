#pragma once

#include <rte_eal.h>
#include <rte_hash.h>

#include <string>
#include <iostream>
#include <list>
#include <set>
#include <rte_random.h>
#include <memory>
#include <sstream>


struct dpdk_settings_s{
    std::set<int> cores;
    int channels;
    int socket_mem;
    bool use_hugepages;
};


 const int def_cores[] ={1,2};
typedef dpdk_settings_s dpdk_settings_t;
dpdk_settings_t default_dpdk_settings = {.cores={1,2}, .channels=4, .socket_mem=2048, .use_hugepages=false};



class DpdkContext final {
    private:
        char **argv;
        int argc;
        DpdkContext(dpdk_settings_t settings) { 
            std::stringstream core_stream;
            for (int c: settings.cores){
                core_stream <<  "," << c;
            }
            argc=settings.use_hugepages?6:5;
            argv = new char*[argc];
            for (int i=0; i<argc; i++){
                argv[i] = new char[500];
            }
            strcpy(argv[0], "-l");
            strcpy(argv[1], &core_stream.str().c_str()[1]);
            strcpy(argv[2], "-n");
            strcpy(argv[3], std::to_string(settings.channels).c_str());
            
            if (!settings.use_hugepages){
                strcpy(argv[4], "--no-huge");
            } else {
                strcpy(argv[4], "--socket-mem");
                strcpy(argv[5], std::to_string(settings.socket_mem).c_str());
            }
        


            std::cout << "initialize dpdk EAL";
            for (int i=0; i<argc; i++){
                std::cout << argv[i] << " ";
            }
            std::cout << std::endl;

            int ret = rte_eal_init(argc, argv);
            std::cout << "rte_eal_init returned: " << ret <<std::endl;
            rte_srand (0);
        }
public:

    ~DpdkContext() {  
        int ret = rte_eal_cleanup();
        for (int i=0; i<argc; i++){
            delete argv[i];
        }
        delete argv;
        std::cout << "rte_eal_cleanup returned: " << ret <<std::endl;
    }

    static DpdkContext& instance(dpdk_settings_s settings=default_dpdk_settings){
        static const std::unique_ptr<DpdkContext> instance{new DpdkContext{settings}};
        return *instance;
    }
};
