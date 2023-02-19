#include <string>
#include <memory>
#include <mutex>
#include <vector>
#include <condition_variable>
#include "redisconnection.hpp"

class RedisConnectionPool {

public:
    /**
     * 初始化连接池
     * @param ip    服务ip
     * @param port  服务端口
     * @param pwd   服务密码
     * @param db    库
     * @param pool_size 连接池大小 
     * @return 
     */
    static bool init(std::string ip, int port, int pool_size = 2, std::string pwd = "", int db = 0);

    /**
     * 获取连接池的一个连接
     * @return 
     */
    static std::shared_ptr<RedisConnection> Get();

    /**
     * 将获取的连接返回连接池
     * @param con 
     */
    static void Back(std::shared_ptr<RedisConnection> &con);

    /**
     * 获取当前连接池可用连接资源数
     * @return 
     */
    static int size();

private:
    static std::mutex _mtx;         //资源锁，防止多线程操作连接池
    static std::vector<std::shared_ptr<RedisConnection>> _connect_pool;  //连接池容器
    static std::condition_variable _noEmpty;
};