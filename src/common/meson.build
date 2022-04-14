common = library('common', sources : ['mymempool.hpp', 'DpdkContext.hpp'],
                dependencies : [])


common_dep = declare_dependency(
    include_directories : include_directories('.'),
    #dependencies: dpdk_dep,
    link_with : [common,dpdk_dep]
    )
