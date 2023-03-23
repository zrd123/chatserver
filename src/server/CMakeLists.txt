# 该目录下的所有源文件
aux_source_directory(. SRC_LIST)
aux_source_directory(./db DB_LIST)
aux_source_directory(./model MODEL_LIST)
aux_source_directory(./redis REDIS_LIST)
aux_source_directory(./protobuf PROTOBUF_LIST)
aux_source_directory(./encryption ENCRYPT_LIST)

# 指定生成可执行文件
add_executable(ChatServer ${SRC_LIST} ${DB_LIST} ${MODEL_LIST} ${REDIS_LIST} ${PROTOBUF_LIST} ${ENCRYPT_LIST})

link_libraries("/usr/lib64")
# add_definitions("-D_GLIBCXX_USE_CXX11_ABI=0")

# 指定库文件
target_link_libraries(ChatServer muduo_net muduo_base mysqlclient hiredis pthread protobuf crypto ssl)