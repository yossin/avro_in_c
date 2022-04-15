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
#include <rte_memzone.h>

struct dpdk_settings_s{
    std::set<int> cores;
    int channels;
    int socket_mem;
    bool use_hugepages;
    bool primary;
};


 const int def_cores[] ={1,2};
typedef dpdk_settings_s dpdk_settings_t;
dpdk_settings_t default_dpdk_settings = {.cores={1,2}, .channels=4, .socket_mem=2048, .use_hugepages=false, .primary=true};



class DpdkContext final {
    private:
        char **argv;
        int argc=0;
        DpdkContext(dpdk_settings_t settings):dpdk_settings(settings) { 
            std::stringstream core_stream;
            for (int c: settings.cores){
                core_stream <<  "," << c;
            }
            argv = new char*[15];
            for (int i=0; i<15; i++){
                argv[i] = new char[500];
            }
            strcpy(argv[argc++], "-l");
            strcpy(argv[argc++], &core_stream.str().c_str()[1]);
            strcpy(argv[argc++], "-n");
            strcpy(argv[argc++], std::to_string(settings.channels).c_str());
            
            if (!settings.use_hugepages){
                strcpy(argv[argc++], "--no-huge");
            } else {
                strcpy(argv[argc++], "--socket-mem");
                strcpy(argv[argc++], std::to_string(settings.socket_mem).c_str());
            }
            strcpy(argv[argc++], "--proc-type");
            strcpy(argv[argc++], settings.primary?"primary":"secondary");
            strcpy(argv[argc++], "--base-virtaddr");
            strcpy(argv[argc++], "0x100000000");
            strcpy(argv[argc++], "--legacy-mem");



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
    const dpdk_settings_t dpdk_settings;

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
