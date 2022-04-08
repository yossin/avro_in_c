#pragma once

#include <rte_eal.h>
#include <rte_hash.h>


#include <iostream> 
#include <list>
#include <array>
#include "Singleton.hpp"
#include <rte_random.h>

template <class T, std::size_t N>
std::ostream& operator<<(std::ostream& o, const std::array<T, N>& arr)
{
    for (auto x : arr){
        o << x << " ";
    }
    return o;
}

class DpdkContext final : public Singleton<DpdkContext>
{
public:
    DpdkContext(token) { 
        std::array<char*,6> init_params={(char*)"-l",(char*) "2-7", (char*)"-n", (char*)"4", (char*)"--socket-mem", (char*)"2048"};
        std::cout << "initialize dpdk EAL, params: " << init_params << std::endl;
        int ret = rte_eal_init(init_params.size(), init_params.data());
        std::cout << "rte_eal_init returned: " << ret <<std::endl;
        rte_srand (0);
    }
    ~DpdkContext() {  
        int ret = rte_eal_cleanup();
        std::cout << "rte_eal_cleanup returned: " << ret <<std::endl;
    }
};
