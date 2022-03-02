
gen_server(raft TestCount)

add_dependencies(TestCount CountServer)

target_link_libraries(TestCount count raft rafttest gtest rocksdb snappy)


