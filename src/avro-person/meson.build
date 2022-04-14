#avro = dependency('avro', version : '>=1.10.2')
#boost_filesystem_dep = dependency('boost', modules : ['filesystem', 'system'])

#avro_inc = include_directories('/usr/local/include')
#avro_lib = shared_library('avro')
#avro_dep = declare_dependency(link_with : avro_lib, include_directories : avro_inc)

#avro_dep = dependency('cmake', method : 'avro', modules : ['avro::avro'])
avro_dep = meson.get_compiler('cpp').find_library('avro', required : true) 

executable('avro-person', 
  sources : ['main.cpp'],
  dependencies : [ avro_dep, utils_dep])#,
  #link_with:dpdk_libraries

