# chatserver
一个基于Linux的C++即时通讯系统，采用muduo库、mysql、redis构建，在nginx tcp负载均衡环境的集群系统下工作。

编译方式
cd build
rm -rf * #删掉不需要的中间文件
cmake ..
make
