#include "chatserver.hpp"
#include "chatservice.hpp"
#include <functional>
#include <string>
#include <muduo/base/Logging.h>
#include <openssl/aes.h>
#include "others.pb.h"
#include "encryption.hpp"

using namespace std;
using namespace placeholders;
using json = nlohmann::json;

ChatServer::ChatServer(EventLoop *loop,
                const InetAddress &listenAddr,
                const string &nameArg)
        :_server(loop, listenAddr, nameArg), _loop(loop)
{
    // 注册链接回调函数
    _server.setConnectionCallback(std::bind(&ChatServer::onConnection,this,_1));

    // 注册消息回调函数
    _server.setMessageCallback(std::bind(&ChatServer::onMessage, this, _1, _2, _3));

    // 设置线程数量
    _server.setThreadNum(4);
}

//启动服务
void ChatServer::start()
{
    _server.start();
}

//上报链接相关信息的回调函数
void ChatServer::onConnection(const TcpConnectionPtr &conn)
{
    if(!conn->connected()){
        ChatService::instance()->clientCloseException(conn);
        conn->shutdown();
    }
}

    //上报读写事件相关信息的回调函数
void ChatServer::onMessage(const TcpConnectionPtr &conn, Buffer *buffer, Timestamp time)
{
    // string buf(buffer->retrieveAllAsString());
    string length(buffer->retrieveAsString(sizeof(uint32_t)));
    // buffer->peekInt32();
    uint32_t len = *(uint32_t*)length.c_str();
    // string rdmIv(buffer->retrieveAsString(AES_BLOCK_SIZE));
    std::cout << "receive length: " << len << "readable bytes:" << buffer->readableBytes() << std::endl;
    string buf(buffer->retrieveAsString(len));
    //数据反序列化
    chat_proto::ChatMessage cmsg;
    if(!cmsg.ParseFromString(buf)){
        LOG_ERROR << buf.size() << (unsigned char *)(cmsg.message_body().c_str());
        LOG_ERROR << "parse error in ChatMessage";
    }

    //达到的目的: 完全节藕网络模块的代码和业务模块的代码
    //通过js["msgid"]获取->业务handler->conn js time
    auto msgHandler = ChatService::instance()->getHandler(cmsg.type());
    //或调消息绑定好的事件处理器,来执行相应的业务处理
    msgHandler(conn, cmsg, time);
}