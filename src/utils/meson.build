utils = library('utils', sources : ['mymempool.hpp', 'DpdkContext.hpp'],
                dependencies : [])


utils_dep = declare_dependency(
    include_directories : include_directories('.'),
    #dependencies: dpdk_dep,
    link_with : [utils,dpdk_dep]
    )
