#ifndef CHATSERVICE_H
#define CHATSERVICE_H

#include <unordered_map>
#include <muduo/net/TcpConnection.h>
#include <functional>
#include <mutex>
#include "redis.hpp"
#include "redisconnectionpool.hpp"
#include "usermodel.hpp"
#include "offlinemessagemodel.hpp"
#include "friendmodel.hpp"
#include "groupmodel.hpp"
#include "json.hpp"
#include "protocol.hpp"

using namespace std;
using namespace muduo;
using namespace muduo::net;
using json = nlohmann::json;
//处理消息回调事件的方法类型
using MsgHandler = std::function<void (const TcpConnectionPtr &conn, chat_proto::ChatMessage &cmsg, Timestamp)>;

//聊天服务器业务类
class ChatService
{
public:
    //获取单例对象
    static ChatService* instance();
    //处理登陆业务
    void login(const TcpConnectionPtr &conn, chat_proto::ChatMessage &cmsg, Timestamp time);
    //处理注册业务
    void reg(const TcpConnectionPtr &conn, chat_proto::ChatMessage &cmsg, Timestamp time);
    //一对一聊天业务
    void oneChat(const TcpConnectionPtr &conn, chat_proto::ChatMessage &cmsg, Timestamp time);
    //添加好友业务
    void addFriendRequest(const TcpConnectionPtr &conn, chat_proto::ChatMessage &cmsg, Timestamp time);
    void addFriendResponse(const TcpConnectionPtr &conn, chat_proto::ChatMessage &cmsg, Timestamp time);
    //创建群组业务
    void createGroup(const TcpConnectionPtr &conn, chat_proto::ChatMessage &cmsg, Timestamp time);
    //加入群组业务
    void addGroupRequest(const TcpConnectionPtr &conn, chat_proto::ChatMessage &cmsg, Timestamp time);
    void addGroupResponse(const TcpConnectionPtr &conn, chat_proto::ChatMessage &cmsg, Timestamp time);
    //群组聊天业务
    void groupChat(const TcpConnectionPtr &conn, chat_proto::ChatMessage &cmsg, Timestamp time);
    //处理注销业务
    void loginout(const TcpConnectionPtr &conn, chat_proto::ChatMessage &cmsg, Timestamp time);
    //获取消息对应的处理器
    MsgHandler getHandler(chat_proto::MessageType);
    //服务器异常,处理方法
    void reset();
    //处理客户端异常退出
    void clientCloseException(const TcpConnectionPtr &conn);
    //从redis消息队列中获取订阅消息
    void handleRedisSubscribeMessage(int userid, std::string msg);
private:
    ChatService();
    //存储消息id和其对应的业务处理方法
    unordered_map<chat_proto::MessageType, MsgHandler> _msgHandlerMap;

    //存储在线用户的通信连接
    unordered_map<int, TcpConnectionPtr> _userConnMap;

    //定义互斥锁,保证_userConnMap的线程安全
    std::mutex _connMutex;

    //数据操作类对象
    UserModel _userModel;
    OfflineMsgModel _offlineMsgModel;
    FriendModel _friendModel;
    GroupModel _groupModel;

    //redis操作对象
    RedisMQ _redis;
    RedisConnectionPool _redisPool;
};

#endif 