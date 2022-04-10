project('cpp_pocs', 'cpp', 
  default_options : ['cpp_std=c++17'],
  version : '0.1')



dpdk = subproject('dpdk')
dpdk_dep = dpdk.get_variable('dpdk_libraries')


subdir('src/utils')
subdir('src/avro-person')
subdir('src/kafka-producer')

