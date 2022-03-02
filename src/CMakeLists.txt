
include_directories(/usr/local/tars/cpp/include)

gen_server(Base CountServer tars)

add_dependencies(CountServer raft)

target_link_libraries(CountServer count raft rocksdb snappy)
