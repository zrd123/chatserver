#include "redisconnectionpool.hpp"
#include <iostream>

std::vector<std::shared_ptr<RedisConnection>> RedisConnectionPool::_connect_pool;
std::mutex RedisConnectionPool::_mtx;
std::condition_variable RedisConnectionPool::_noEmpty;

bool RedisConnectionPool::init(std::string ip, int port, int pool_size, std::string pwd,int db) {
    bool flag = true;
    if(pool_size <= 0){
        flag = false;
        return flag;
    }
    int real_pool_size;

    {
        auto lock = std::unique_lock<std::mutex>(_mtx);
        for (int i = 0; i < pool_size; ++i) {
            //实例化连接
            RedisConnection* con = new RedisConnection();
            if(con->connect(ip,port,pwd,db)){
                //放入连接池
                _connect_pool.emplace_back(std::shared_ptr<RedisConnection>(con));
            }
            else{
                std::cerr << "connect redirs :" <<ip << " :" << port << pwd << " failed";
            }
        }
        real_pool_size = _connect_pool.size();
    }

    if(pool_size > real_pool_size){
        std::cerr << "redis pool init failed! hope pool size:" << pool_size << " real size is " << real_pool_size;
        flag = false;
    }
    else{
        std::cerr << "redis pool init success! pool size:" << real_pool_size;
    }
    return flag;
}

std::shared_ptr<RedisConnection> RedisConnectionPool::Get() 
{
    std::unique_lock<std::mutex> lock(_mtx);
    if(_connect_pool.size() == 0){
        std::cerr << "wait for a redisconnection!";
        _noEmpty.wait(lock);
    }
    //从连接容器里返回一个连接
    std::shared_ptr<RedisConnection> tmp = _connect_pool.front();
    _connect_pool.erase(_connect_pool.begin());
    return tmp;
}

void RedisConnectionPool::Back(std::shared_ptr<RedisConnection> &con) 
{
    std::unique_lock<std::mutex>(_mtx);
    //归还到容器
    _connect_pool.emplace_back(con);
    _noEmpty.notify_one();
}

int RedisConnectionPool::size() 
{
    std::unique_lock<std::mutex>(_mtx);
    int size = _connect_pool.size();
    return size;
}