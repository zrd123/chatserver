#include "chatservice.hpp"
#include "public.hpp"
#include "user.hpp"
#include "encryption.hpp"
#include <iostream>
#include <map>
#include <vector>
#include <muduo/base/Logging.h>
#include <openssl/aes.h>

using std::map;
using std::vector;

//获取单例对象
ChatService *ChatService::instance()
{
    static ChatService service;
    return &service;
}

//注册消息以及对应的Handler回调操作
ChatService::ChatService()
{
    //用户基本业务管理相关事件处理回调注册
    _msgHandlerMap.insert({chat_proto::LOGIN_MSG, std::bind(&ChatService::login, this, _1, _2, _3)});
    _msgHandlerMap.insert({chat_proto::LOGINOUT_MSG, std::bind(&ChatService::loginout, this, _1, _2, _3)});
    _msgHandlerMap.insert({chat_proto::REG_MSG, std::bind(&ChatService::reg, this, _1, _2, _3)});
    _msgHandlerMap.insert({chat_proto::ONE_CHAT_MSG, std::bind(&ChatService::oneChat, this, _1, _2, _3)});
    _msgHandlerMap.insert({chat_proto::ADD_FRIEND_MSG, std::bind(&ChatService::addFriendRequest, this, _1, _2, _3)});
    _msgHandlerMap.insert({chat_proto::ADD_FRIEND_ACK, std::bind(&ChatService::addFriendResponse, this, _1, _2, _3)});

    //群组业务管理相关事件处理回调注册
    _msgHandlerMap.insert({chat_proto::CREATE_GROUP_MSG, std::bind(&ChatService::createGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({chat_proto::ADD_GROUP_MSG, std::bind(&ChatService::addGroupRequest, this, _1, _2, _3)});
    _msgHandlerMap.insert({chat_proto::ADD_GROUP_ACK, std::bind(&ChatService::addGroupResponse, this, _1, _2, _3)});
    _msgHandlerMap.insert({chat_proto::GROUP_CHAT_MSG, std::bind(&ChatService::groupChat, this, _1, _2, _3)});

    if (!_redisPool.init(std::string("127.0.0.1"), 6379)){
        std::cerr << "redis pool initialize failed!";
    };
    //连接redis服务器
    if (_redis.connect(std::string("127.0.0.1"), 6379)){
        //设置上报消息的回调
        _redis.init_notify_handler(std::bind(&ChatService::handleRedisSubscribeMessage, this, _1, _2));
    }
}

//服务器异常,处理方法
void ChatService::reset()
{
    //把online状态的用户,设置成offline
    _userModel.resetState();
}

//获取消息对应的处理器
// MsgHandler ChatService::getHandler(int msgid)
MsgHandler ChatService::getHandler(chat_proto::MessageType msgtype)
{
    auto it = _msgHandlerMap.find(msgtype);
    if (it == _msgHandlerMap.end()){
        //返回一个默认的处理器
        return [=](const TcpConnectionPtr &conn, chat_proto::ChatMessage &cmsg, Timestamp time){
            LOG_ERROR << "msgid" << msgtype << " can not find handler!";
        };
    }
    else{
        return _msgHandlerMap[msgtype];
    }
}

//处理登陆业务
void ChatService::login(const TcpConnectionPtr &conn, chat_proto::ChatMessage &cmsg, Timestamp time)
{
    LOG_INFO << "do login service !";
    chat_proto::LoginRequest loginRst;
    std::string *msg = cmsg.mutable_message_body();
    Encryption::doDecryption(*msg, cmsg.length(), nullptr);
    if (!loginRst.ParseFromString(*msg)){
        LOG_ERROR << "parse error in login service";
    }
    cmsg.set_type(chat_proto::LOGIN_ACK);

    uint32_t id = loginRst.user_id();
    User user = _userModel.query(id);
    LOG_INFO << "length:" << cmsg.length() << "id:" << id;

    if (user.getId() == id && user.getPwd() == loginRst.password()){
        if (user.getState() == "online"){
            //该用户已登陆,不能重复登陆
            chat_proto::LoginResponse loginRsp;
            loginRsp.set_error_status(chat_proto::LOGIN_ERR_2);
            std::string *loginmsgbody = cmsg.mutable_message_body();
            loginRsp.SerializeToString(loginmsgbody);
            uint32_t length = loginmsgbody->size();
            length = (0 == length % AES_BLOCK_SIZE) ? length : ((length/AES_BLOCK_SIZE)+1)*AES_BLOCK_SIZE;
            cmsg.set_length(length);
            Encryption::doEncryption(*loginmsgbody, length, nullptr);
            std::string loginmsg;
            cmsg.SerializeToString(&loginmsg);
            length = loginmsg.size();
            conn->send((char*)&length, sizeof(uint32_t));
            conn->send(loginmsg);    
        }
        else{
            //登陆成功,记录连接信息
            {
                lock_guard<mutex> lock(_connMutex);
                _userConnMap.insert({id, conn});
            }

            // id用户登陆成功后,向redis订阅channel(id)
            _redis.subscribe(id);
            //登陆成功,更新用户状态信息
            user.setState("online");
            _userModel.updateState(user);

            chat_proto::LoginResponse loginRsp;
            loginRsp.set_error_status(chat_proto::NO_ERR);
            chat_proto::User *cuser = loginRsp.mutable_user();
            cuser->set_id(id);
            cuser->set_name(user.getName());
            cuser->set_status(user.getState());
            std::string *loginmsgbody = cmsg.mutable_message_body();

            //查询用户的好友信息
            if (loginRst.request_info()){
                chat_proto::LoadResponse *loadRsp = loginRsp.mutable_user_info();
                vector<chat_proto::User> userVec = _friendModel.query(id);
                if (!userVec.empty()){
                    for (const chat_proto::User &user : userVec){
                        chat_proto::User *cpuser = loadRsp->add_friend_list();
                        cpuser->set_id(user.id());
                        cpuser->set_name(user.name());
                        cpuser->set_status(user.status());
                    }
                }
                //查询用户的群组信息
                vector<chat_proto::Group> groupuserVec = _groupModel.queryGroups(id);
                LOG_INFO << "vector size:" << groupuserVec.size();
                if (!groupuserVec.empty()){
                    for (const chat_proto::Group &group : groupuserVec){
                        chat_proto::Group *cpgroup = loadRsp->add_group_list();
                        cpgroup->set_id(group.id());
                        cpgroup->set_name(group.name());
                        cpgroup->set_description(group.description());
                        cpgroup->set_creator(group.creator());
                        LOG_INFO << "group name:" << cpgroup->name();
                        for (const chat_proto::GroupMember &member : group.members()){
                            chat_proto::GroupMember *cpmember = cpgroup->add_members();
                            chat_proto::User *gmember = cpmember->mutable_user();
                            cpmember->set_role(member.role());
                            gmember->set_id(member.user().id());
                            gmember->set_name(member.user().name());
                            gmember->set_status(member.user().status());
                            LOG_INFO << gmember->id() << gmember->status() << gmember->name() << cpmember->role();
                        }
                    }
                    LOG_INFO << "group_list:" << loadRsp->group_list_size();
                }
                loginRsp.SerializeToString(loginmsgbody);
                uint32_t length = loginmsgbody->size();
                length = (0 == length % AES_BLOCK_SIZE) ? length : ((length/AES_BLOCK_SIZE)+1)*AES_BLOCK_SIZE;
                cmsg.set_length(length);
                Encryption::doEncryption(*loginmsgbody, length, nullptr);
            }
            std::string loginmsg;
            cmsg.SerializeToString(&loginmsg);
            uint32_t slength = loginmsg.size();
            
            std::cout << "send length: " << slength <<" " << cmsg.ByteSizeLong() << std::endl;
            conn->send((char*)&slength, sizeof(uint32_t));
            conn->send(loginmsg);
            //查询是否有离线消息
            vector<string> vec = _offlineMsgModel.query(id);
            for (std::string &msg : vec){
                std::cout << "--------------------msg:" << msg << std::endl;
                uint32_t slengthmsg = msg.size();
                conn->send((char*)&slengthmsg, sizeof(uint32_t));
                conn->send(msg);
            }
            if (!vec.empty()){
                //读取离线消息后,把用户的离线消息删除掉
                _offlineMsgModel.remove(id);
            }
        }
    }
    else{
        //用户不存在,或密码错误,登录失败
        chat_proto::LoginResponse loginRsp;
        loginRsp.set_error_status(chat_proto::LOGIN_ERR_3);
        std::string *loginmsgbody = cmsg.mutable_message_body();
        loginRsp.SerializeToString(loginmsgbody);
        uint32_t length = loginmsgbody->size();
        length = (0 == length % AES_BLOCK_SIZE) ? length : ((length/AES_BLOCK_SIZE)+1)*AES_BLOCK_SIZE;
        cmsg.set_length(length);
        Encryption::doEncryption(*loginmsgbody, length, nullptr);
        std::string loginmsg;
        cmsg.SerializeToString(&loginmsg);

        uint32_t slength = loginmsg.size();
        conn->send((char*)&slength, sizeof(uint32_t));
        conn->send(loginmsg);    
    }
}

//处理注册业务
void ChatService::reg(const TcpConnectionPtr &conn, chat_proto::ChatMessage &cmsg, Timestamp time)
{
    chat_proto::RegisterRequest regRst;
    std::string *msg = cmsg.mutable_message_body();
    Encryption::doDecryption(*msg, cmsg.length(), nullptr);
    if (!regRst.ParseFromString(*msg)){
        LOG_ERROR << "parse error in login service";
    }
    User user;
    user.setName(regRst.user_name());
    user.setPwd(regRst.password());
    bool state = _userModel.insert(user);

    cmsg.set_type(chat_proto::REG_ACK);
    chat_proto::RegisterResponse regRsp;
    if (state){
        //注册成功
        regRsp.set_error_status(chat_proto::NO_ERR);
        regRsp.set_user_id(user.getId());
    }
    else{
        //注册失败
        regRsp.set_error_status(chat_proto::REG_ERR_1);
    }
    std::string *regmsgbody = cmsg.mutable_message_body();
    regRsp.SerializeToString(regmsgbody);
    //加密处理
    uint32_t length = regmsgbody->size();
    length = (0 == length % AES_BLOCK_SIZE) ? length : ((length/AES_BLOCK_SIZE)+1)*AES_BLOCK_SIZE;
    cmsg.set_length(length);
    Encryption::doEncryption(*regmsgbody, length, nullptr);
    //最终序列化
    std::string regmsg;
    cmsg.SerializeToString(&regmsg);
    length = regmsg.size();
    std::cout << "send length: " << length << std::endl;
    std::cout << "sizeof " << sizeof((char*)&length)  << sizeof(uint32_t)<< std::endl;
    conn->send((char*)&length, sizeof(uint32_t));
    conn->send(regmsg);
}

//处理注销业务
void ChatService::loginout(const TcpConnectionPtr &conn, chat_proto::ChatMessage &cmsg, Timestamp time)
{
    chat_proto::LoginoutRequest logoutRst;
    std::string *msg = cmsg.mutable_message_body();
    Encryption::doDecryption(*msg, cmsg.length(), nullptr);
    if (!logoutRst.ParseFromString(*msg)){
        LOG_ERROR << "parse error in loginout service";
    }
    uint32_t userid = logoutRst.user_id();
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(userid);
        if (it != _userConnMap.end()){
            _userConnMap.erase(it);
        }
    }

    //用户注销下线,在redis中取消订阅通道
    _redis.unsubscribe(userid);

    //更新用户的状态信息
    User user(userid, "", "", "offline");
    _userModel.updateState(user);
}

//处理客户端异常退出
void ChatService::clientCloseException(const TcpConnectionPtr &conn)
{
    User user;
    {
        lock_guard<mutex> lock(_connMutex);
        for (auto it = _userConnMap.begin(); it != _userConnMap.end(); ++it){
            if (conn == it->second){
                //删除用户的连接信息
                user.setId(it->first);
                _userConnMap.erase(it);
                break;
            }
        }
    }
    //用户注销下线,在redis中取消订阅通道
    _redis.unsubscribe(user.getId());

    if (user.getId() != -1){
        //更新登陆状态
        user.setState("offline");
        _userModel.updateState(user);
    }
}

//一对一聊天业务
void ChatService::oneChat(const TcpConnectionPtr &conn, chat_proto::ChatMessage &cmsg, Timestamp time)
{
    // int toId = js["toid"].get<int>();
    chat_proto::OneChatRequest ochatRst;
    std::string msg = cmsg.message_body();
    Encryption::doDecryption(msg, cmsg.length(), nullptr);
    std::cout << "in one chat: " << cmsg.message_body() << std::endl;
    if (!ochatRst.ParseFromString(msg)){
        LOG_ERROR << "parse error in onchat service";
    }

    std::string ocmsg;
    cmsg.SerializeToString(&ocmsg);
    uint32_t length = ocmsg.size();
    uint32_t toId = ochatRst.to_id();
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(toId);
        if (_userConnMap.end() != it){
            // toid在线,在本服务器,转发消息 服务器主动推送消息给toId用户
            std::cout << "send length: " << length << std::endl;
            it->second->send((char*)&length, sizeof(uint32_t));
            it->second->send(ocmsg);
            return;
        }
    }

    //查询toid是否在线,是否在其他服务器
    if (_userModel.cacheQuery(toId) == "online" || _userModel.query(toId).getState() == "online"){
        _redis.publish(toId, ocmsg);
        return;
    }

    // toid不在现,存储离线消息
    _offlineMsgModel.insert(toId, ocmsg);
}

//添加好友业务
void ChatService::addFriendRequest(const TcpConnectionPtr &conn, chat_proto::ChatMessage &cmsg, Timestamp time)
{
    chat_proto::AddFriendRequest addfRst;
    std::string msg = cmsg.message_body();
    Encryption::doDecryption(msg, cmsg.length(), nullptr);
    if (!addfRst.ParseFromString(msg)){
        LOG_ERROR << "parse error in addfriend request service";
    }
    uint32_t userid = addfRst.user_id();
    uint32_t friendid = addfRst.friend_id();

    std::string afmsg;
    cmsg.SerializeToString(&afmsg);
    uint32_t length = afmsg.size();
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(friendid);
        if (_userConnMap.end() != it){
            // toid在线,在本服务器,转发消息 服务器主动推送消息给toId用户
            std::cout << "send length in add friend request: " << length  << " friendid" << friendid << std::endl;
            it->second->send((char*)&length, sizeof(uint32_t));
            it->second->send(afmsg);            
            return;
        }
    }

    //查询toid是否在线,是否在其他服务器
    if (_userModel.cacheQuery(friendid) == "online" || _userModel.query(friendid).getState() == "online"){
        _redis.publish(friendid, afmsg);
        return;
    }

    // toid不在现,存储离线消息
    _offlineMsgModel.insert(friendid, afmsg);
}

void ChatService::addFriendResponse(const TcpConnectionPtr &conn, chat_proto::ChatMessage &cmsg, Timestamp time)
{
    chat_proto::AddFriendResponse addfRsp;
    std::string msg = cmsg.message_body();
    Encryption::doDecryption(msg, cmsg.length(), nullptr);
    if (!addfRsp.ParseFromString(msg)){
        LOG_ERROR << "parse error in addfriend response service";
    }
    uint32_t userid = addfRsp.user_id();
    uint32_t friendid = addfRsp.friend_id();
    if (addfRsp.pass()){
        //存储好友信息
        _friendModel.insert(userid, friendid);
        _friendModel.insert(friendid, userid);
    }

    std::string afmsg;
    cmsg.SerializeToString(&afmsg);
    uint32_t length = afmsg.size();
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(userid);
        if (_userConnMap.end() != it){
            // toid在线,在本服务器,转发消息 服务器主动推送消息给toId用户
            std::cout << "send length: " << length << std::endl;
            it->second->send((char*)&length, sizeof(uint32_t));
            it->second->send(afmsg);
            return;
        }
    }

    //查询toid是否在线,是否在其他服务器
    if (_userModel.cacheQuery(userid) == "online" || _userModel.query(userid).getState() == "online"){
        _redis.publish(userid, afmsg);
        return;
    }

    // toid不在现,存储离线消息
    _offlineMsgModel.insert(userid, afmsg);
}

//创建群组业务
void ChatService::createGroup(const TcpConnectionPtr &conn, chat_proto::ChatMessage &cmsg, Timestamp time)
{
    chat_proto::CreateGroupRequest creategRst;
    std::string *msg = cmsg.mutable_message_body();
    Encryption::doDecryption(*msg, cmsg.length(), nullptr);
    if (!creategRst.ParseFromString(*msg)) {
        LOG_ERROR << "parse error in creategroup request service";
    }
    //存储新创建的群组信息
    chat_proto::Group group;
    group.set_name(creategRst.group_name());
    group.set_description(creategRst.group_description());
    group.set_id(0);
    if (_groupModel.createGroup(group, creategRst.user_id())){
        //存储群创建人信息
        _groupModel.addGroup(creategRst.user_id(), group.id(), "creator");
    }
    chat_proto::CreateGroupResponse creategRsp;
    creategRsp.set_group_id(group.id());
    creategRsp.set_error_status(chat_proto::NO_ERR);

    // chat_proto::ChatMessage chatmsg;
    cmsg.set_type(chat_proto::CREATE_GROUP_ACK);
    std::string *cgmsgbody = cmsg.mutable_message_body();
    creategRsp.SerializeToString(cgmsgbody);
    //加密处理
    uint32_t length = cgmsgbody->size();
    length = (0 == length % AES_BLOCK_SIZE) ? length : ((length/AES_BLOCK_SIZE)+1)*AES_BLOCK_SIZE;
    cmsg.set_length(length);
    Encryption::doEncryption(*cgmsgbody, length, nullptr);
    //最终序列化
    std::string cgmsg;
    cmsg.SerializeToString(&cgmsg);
    length = cgmsg.size();
    std::cout << "send length: " << length << std::endl;
    std::cout << "sizeof " << sizeof((char*)&length)  << sizeof(uint32_t)<< std::endl;
    conn->send((char*)&length, sizeof(uint32_t));
    conn->send(cgmsg);  
}

//加入群组业务
void ChatService::addGroupRequest(const TcpConnectionPtr &conn, chat_proto::ChatMessage &cmsg, Timestamp time)
{
    chat_proto::AddGroupRequest addgRst;
    std::string msg = cmsg.message_body();
    Encryption::doDecryption(msg, cmsg.length(), nullptr);
    if (!addgRst.ParseFromString(msg)){
        LOG_ERROR << "parse error in addgroup request service";
    }
    uint32_t groupid = addgRst.group_id();
    uint32_t creator = _groupModel.creator(groupid);

    LOG_ERROR << "cretor id:" << creator;
    if(0 == creator){
        //群组不存在
    }
    std::string addgmsg;
    cmsg.SerializeToString(&addgmsg);
    uint32_t length = addgmsg.size();
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(creator);
        if (_userConnMap.end() != it){
            // toid在线,在本服务器,转发消息 服务器主动推送消息给toId用户
            std::cout << "send length: " << length << std::endl;
            it->second->send((char*)&length, sizeof(uint32_t));
            it->second->send(addgmsg);
            return;
        }
    }

    //查询toid是否在线,是否在其他服务器
    if (_userModel.cacheQuery(creator) == "online" || _userModel.query(creator).getState() == "online"){
        _redis.publish(creator, addgmsg);
        return;
    }

    // toid不在现,存储离线消息
    _offlineMsgModel.insert(creator, addgmsg);  
}

void ChatService::addGroupResponse(const TcpConnectionPtr &conn, chat_proto::ChatMessage &cmsg, Timestamp time)
{
    chat_proto::AddGroupResponse addgRsp;
    std::string msg = cmsg.message_body();
    Encryption::doDecryption(msg, cmsg.length(), nullptr);
    if (!addgRsp.ParseFromString(msg)){
        LOG_ERROR << "parse error in addgroup response service";
    }
    uint32_t userid = addgRsp.user_id();
    LOG_ERROR << "userid: " << userid << " groupid:" << addgRsp.group_id();
    if (addgRsp.pass()){
        _groupModel.addGroup(userid, addgRsp.group_id(), "normal");
    }
    std::string agmsg;
    cmsg.SerializeToString(&agmsg);
    uint32_t length = agmsg.size();
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(userid);
        if (_userConnMap.end() != it){
            // toid在线,在本服务器,转发消息 服务器主动推送消息给toId用户
            std::cout << "send length: " << length << std::endl;
            it->second->send((char*)&length, sizeof(uint32_t));
            it->second->send(agmsg);
            return;
        }
    }

    //查询toid是否在线,是否在其他服务器
    if (_userModel.cacheQuery(userid) == "online" || _userModel.query(userid).getState() == "online"){
        _redis.publish(userid, agmsg);
        return;
    }

    // toid不在现,存储离线消息
    _offlineMsgModel.insert(userid, agmsg);
}

//群组聊天业务
void ChatService::groupChat(const TcpConnectionPtr &conn, chat_proto::ChatMessage &cmsg, Timestamp time)
{
    chat_proto::GroupChatRequest gcRst;
    std::string msg = cmsg.message_body();
    Encryption::doDecryption(msg, cmsg.length(), nullptr);
    if (!gcRst.ParseFromString(msg)){
        LOG_ERROR << "parse error in groupchat request service";
    }

    vector<uint32_t> useridVec = _groupModel.queryGroupUsers(gcRst.user_id(), gcRst.group_id());
    std::string gcmsg;
    cmsg.SerializeToString(&gcmsg);
    uint32_t length = gcmsg.size();

    lock_guard<mutex> lock(_connMutex);
    for (const int &id : useridVec){
        auto it = _userConnMap.find(id);
        if (it != _userConnMap.end()){
            //转发群消息
            std::cout << "in group chat send length: " << length << std::endl;
            std::cout << "sizeof " << sizeof((char*)&length)  << sizeof(uint32_t)<< std::endl;
            it->second->send((char*)&length, sizeof(uint32_t));
            it->second->send(gcmsg); 
        }
        else{
            //查询toid是否在线
            if (_userModel.cacheQuery(id) == "online" || _userModel.query(id).getState() == "online"){
                _redis.publish(id, gcmsg);
            }
            else{
                //存储离线群消息
                _offlineMsgModel.insert(id, gcmsg);
            }
        }
    }
}

//从redis消息队列中获取订阅消息
void ChatService::handleRedisSubscribeMessage(int userid, string msg)
{
    lock_guard<std::mutex> lock(_connMutex);
    auto it = _userConnMap.find(userid);
    uint32_t length = msg.size();
    length = (0 == length % AES_BLOCK_SIZE) ? length : ((length/AES_BLOCK_SIZE)+1)*AES_BLOCK_SIZE;
    Encryption::doEncryption(msg, length, nullptr);
    if (it != _userConnMap.end()){
            std::cout << "send length: " << length << std::endl;
            std::cout << "sizeof " << sizeof((char*)&length)  << sizeof(uint32_t)<< std::endl;
            it->second->send((char*)&length, sizeof(uint32_t));
            it->second->send(msg);    
        // it->second->send(msg);
        return;
    }

    //可能下线,存储该用户的离线消息
    _offlineMsgModel.insert(userid, msg);
}
