#include "redis.hpp"
#include <iostream>

RedisMQ::RedisMQ()
    :_publish_context(nullptr), _subscribe_context(nullptr)
{

}

RedisMQ::~RedisMQ()
{
    if(_publish_context){
        redisFree(_publish_context);
    } 
    if(_subscribe_context){
        redisFree(_subscribe_context);
    }
}

//连接redis服务器
bool RedisMQ::connect(const std::string &ip, int port)
{
    _publish_context = redisConnect(ip.c_str(), port);
    if(nullptr == _publish_context){
        std::cerr << "connect redis failed!" << std::endl; 
        return false;
    }
    
    _subscribe_context = redisConnect(ip.c_str(), port);
    if(nullptr == _subscribe_context){
        std::cerr << "connect redis failed!" << std::endl;
        return false;
    }

    //在单独线程监听通道事件,有消息就给业务层上报
    std::thread t([&](){
        observer_channel_message();
    });
    t.detach();

    std::cout << "connect redis-server successfully!" << std::endl;
    return true;
}

//向redis指定的通道channel发布消息
bool RedisMQ::publish(int channel, std::string message)
{
    redisReply *reply = static_cast<redisReply *>(
        redisCommand(_publish_context, "PUBLISH %d %s", channel, message.c_str()
    ));
    if(nullptr == reply){
        std::cerr << "publish command failed!" << std::endl;
        return false;
    }
    freeReplyObject(reply);
    return true;
}

//向redis指定的通道subscribe订阅消息
bool RedisMQ::subscribe(int channel)
{
    //SUBSCRIBE命令本身会造成线程的阻塞等待通道里面发生消息,这里只做订阅通道,不接受同消息
    //通道消息的接收专门在observer_channel_message函数中的独立线程中进行
    //只负责发送命令,不阻塞接收redis server响应消息,否则和notifyMsg线程抢占资源
    if(REDIS_ERR == redisAppendCommand(this->_subscribe_context, "SUBSCRIBE %d", channel)){
        std::cerr << "subscribe command failed!"  << std::endl;
        return false;
    }
    //redisBufferWrite可以循环发送缓冲区,直到缓冲区数据发送完毕
    int done = 0;
    while(!done){
        if(REDIS_ERR == redisBufferWrite(this->_subscribe_context, &done)){
            std::cerr << "subscribe command failed!" << std::endl;
            return false;
        }
    }

    return true;
}

//向redis指定的通道unsubscribe取消订阅消息
bool RedisMQ::unsubscribe(int channel)
{
    if(REDIS_ERR == redisAppendCommand(this->_subscribe_context, "UNSUBSCRIBE %d", channel)){
        std::cerr << "unsubscribe command failed!"  << std::endl;
        return false;
    }
    //redisBufferWrite可以循环发送缓冲区,直到缓冲区数据发送完毕(done 被设置为1)
    int done = 0;
    while(!done){
        if(REDIS_ERR == redisBufferWrite(this->_subscribe_context, &done)){
            std::cerr << "unsubscribe command failed!" << std::endl;
            return false;
        }
    }
    return true;
}

//在独立线程中接收订阅通道中的消息
void RedisMQ::observer_channel_message()
{
    redisReply *reply = nullptr;
    while(REDIS_OK == redisGetReply(this->_subscribe_context, (void **)&reply)){
        if(nullptr != reply && nullptr != reply->element[2] && nullptr != reply->element[2]->str){
            //给业务层上报通道消息
            _notify_message_handler(atoi(reply->element[1]->str), reply->element[2]->str);
        }
        freeReplyObject(reply);
    }
    std::cerr << ">>>>>>>>>>>>>>> observer_channel_message quit <<<<<<<<<<<<<<<" << std::endl;
}

//初始化向业务层上报通道消息的回调对象
void RedisMQ::init_notify_handler(std::function<void(int, std::string)> fn)
{
    this->_notify_message_handler = fn;
}