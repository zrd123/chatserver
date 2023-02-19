#include "chatservice.hpp"
#include "public.hpp"
#include "user.hpp"

#include <iostream>
#include <map>
#include <vector>
#include <muduo/base/Logging.h>

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

    if (!_redisPool.init(std::string("127.0.0.1"), 6379))
    {
        std::cerr << "redis pool initialize failed!";
    };
    //连接redis服务器
    if (_redis.connect(std::string("127.0.0.1"), 6379))
    {
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
    if (it == _msgHandlerMap.end())
    {
        //返回一个默认的处理器
        return [=](const TcpConnectionPtr &conn, chat_proto::ChatMessage &cmsg, Timestamp time)
        {
            LOG_ERROR << "msgid" << msgtype << " can not find handler!";
        };
    }
    else
    {
        return _msgHandlerMap[msgtype];
    }
}

//处理登陆业务
void ChatService::login(const TcpConnectionPtr &conn, chat_proto::ChatMessage &cmsg, Timestamp time)
{
    LOG_INFO << "do login service !";
    chat_proto::LoginRequest loginRst;
    if (!loginRst.ParseFromString(cmsg.message_body()))
    {
        LOG_ERROR << "parse error in login service";
    }
    // int id = js["id"].get<int>();
    // string pwd = js["password"];

    unsigned int id = loginRst.user_id();
    User user = _userModel.query(id);
    if (user.getId() == id && user.getPwd() == loginRst.password())
    {
        if (user.getState() == "online")
        {
            //该用户已登陆,不能重复登陆
            chat_proto::ChatMessage chatmsg;
            chatmsg.set_type(chat_proto::LOGIN_MSG_ACK);
            chat_proto::LoginResponse loginRsp;
            loginRsp.set_error_status(chat_proto::LOGIN_ERR_2);
            std::string *loginmsgbody = chatmsg.mutable_message_body();
            loginRsp.SerializeToString(loginmsgbody);
            std::string loginmsg;
            chatmsg.SerializeToString(&loginmsg);
            // json response;
            // response["msgid"] = LOGIN_MSG_ACK;
            // response["errno"] = 2;
            // response["errmsg"] = "该账号已登陆，清重新登陆新账号";
            conn->send(loginmsg);
        }
        else
        {
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

            chat_proto::ChatMessage chatmsg;
            chatmsg.set_type(chat_proto::LOGIN_MSG_ACK);

            chat_proto::LoginResponse loginRsp;
            loginRsp.set_error_status(chat_proto::NO_ERR);
            std::string *loginmsgbody = chatmsg.mutable_message_body();
            loginRsp.SerializeToString(loginmsgbody);

            std::string loginmsg;
            chatmsg.SerializeToString(&loginmsg);
            conn->send(loginmsg);

            // json response;
            // response["msgid"] = LOGIN_MSG_ACK;
            // response["errno"] = 0;
            // response["id"] = user.getId();
            // response["name"] = user.getName();
            //查询用户的好友信息
            chat_proto::ChatMessage chatloadmsg;
            chatloadmsg.set_type(chat_proto::LOAD_ACK);
            chat_proto::LoadResponse loadRsp;
            vector<User> userVec = _friendModel.query(id);
            if (!userVec.empty())
            {
                // vector<string> friendVec;
                for (const User &user : userVec)
                {
                    chat_proto::User *cpuser = loadRsp.add_friend_list();
                    cpuser->set_id(user.getId());
                    cpuser->set_name(user.getName());
                    cpuser->set_status(user.getState());
                    // json js;
                    // js["id"] = user.getId();
                    // js["name"] = user.getName();
                    // js["state"] = user.getState();
                    // friendVec.push_back(js.dump());
                }
            }
            //查询用户的群组信息
            vector<Group> groupuserVec = _groupModel.queryGroups(id);
            if (!groupuserVec.empty())
            {
                // group:[{groupid:[xxx, xxx, xxx, xxx]}]
                // vector<string> groupV;
                for (Group &group : groupuserVec)
                {
                    chat_proto::Group *cpgroup = loadRsp.add_group_list();
                    cpgroup->set_id(group.getId());
                    cpgroup->set_name(group.getName());
                    cpgroup->set_description(group.getDesc());
                    // json grpjson;
                    // grpjson["id"] = group.getId();
                    // grpjson["groupname"] = group.getName();
                    // grpjson["groupdesc"] = group.getDesc();
                    // vector<string> userV;
                    for (GroupUser &user : group.getUsers())
                    {
                        chat_proto::GroupMember *cpmember = cpgroup->add_members();
                        chat_proto::User *gmember = cpmember->mutable_user();
                        cpmember->set_role(user.getRole());
                        gmember->set_id(user.getId());
                        gmember->set_name(user.getName());
                        gmember->set_status(user.getState());
                        // json js;
                        // js["id"] = user.getId();
                        // js["name"] = user.getName();
                        // js["state"] = user.getState();
                        // js["role"] = user.getRole();
                        // userV.push_back(js.dump());
                    }
                    // grpjson["users"] = userV;
                    // groupV.push_back(grpjson.dump());
                }
                // response["groups"] = groupV;
            }
            std::string *loadmsgbody = chatloadmsg.mutable_message_body();
            loadRsp.SerializeToString(loadmsgbody);
            std::string loadmsg;
            chatloadmsg.SerializeToString(&loadmsg);
            conn->send(loadmsg);
            //查询是否有离线消息
            vector<string> vec = _offlineMsgModel.query(id);
            for(const std::string & msg : vec){
                conn->send(msg);
            }
            if (!vec.empty())
            {
                // response["offlinemsg"] = vec;
                //读取离线消息后,把用户的离线消息删除掉
                _offlineMsgModel.remove(id);
            }
            // conn->send(response.dump());
        }
    }
    else
    {
        //用户不存在,或密码错误,登录失败
        chat_proto::LoginResponse loginRsp;
        loginRsp.set_error_status(chat_proto::LOGIN_ERR_3);
        std::string loginmsg;
        loginRsp.SerializeToString(&loginmsg);
        // json response;
        // response["msgid"] = LOGIN_MSG_ACK;
        // response["errno"] = 1;
        // response["errmsg"] = "用户不存在或密码错误";
        conn->send(loginmsg);
    }
}

//处理注册业务
void ChatService::reg(const TcpConnectionPtr &conn, chat_proto::ChatMessage &cmsg, Timestamp time)
{
    // string name = js["name"];
    // string pwd = js["password"];

    chat_proto::RegisterRequest regRst;
    if (!regRst.ParseFromString(cmsg.message_body()))
    {
        LOG_ERROR << "parse error in login service";
    }
    User user;
    user.setName(regRst.user_name());
    user.setPwd(regRst.password());
    bool state = _userModel.insert(user);

    chat_proto::ChatMessage chatmsg;
    chatmsg.set_type(chat_proto::REG_MSG_ACK);
    chat_proto::RegisterResponse regRsp;
    if (state)
    {
        //注册成功
        regRsp.set_error_status(chat_proto::NO_ERR);
        regRsp.set_user_id(user.getId());
        // json response;
        // response["msgid"] = REG_MSG_ACK;
        // response["errno"] = 0;
        // response["id"] = user.getId();
        // conn->send(response.dump());
    }
    else
    {
        regRsp.set_error_status(chat_proto::REG_ERR_1);
        // json response;
        // response["msgid"] = REG_MSG_ACK;
        // response["errno"] = 1;
        // conn->send(response.dump());
        //注册失败
    }
    std::string *regmsgbody = chatmsg.mutable_message_body();
    regRsp.SerializeToString(regmsgbody);
    std::string regmsg;
    chatmsg.SerializeToString(&regmsg);
    conn->send(regmsg);
}

//处理注销业务
void ChatService::loginout(const TcpConnectionPtr &conn, chat_proto::ChatMessage &cmsg, Timestamp time)
{
    // int userid = js["id"].get<int>();
    chat_proto::LoginoutRequest logoutRst;
    if (!logoutRst.ParseFromString(cmsg.message_body()))
    {
        LOG_ERROR << "parse error in loginout service";
    }
    uint32_t userid = logoutRst.user_id();

    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(userid);
        if (it != _userConnMap.end())
        {
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
        for (auto it = _userConnMap.begin(); it != _userConnMap.end(); ++it)
        {
            if (conn == it->second)
            {
                //删除用户的连接信息
                user.setId(it->first);
                _userConnMap.erase(it);
                break;
            }
        }
    }

    //用户注销下线,在redis中取消订阅通道
    _redis.unsubscribe(user.getId());

    if (user.getId() != -1)
    {
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
    if (!ochatRst.ParseFromString(cmsg.message_body()))
    {
        LOG_ERROR << "parse error in onchat service";
    }
    uint32_t toId = ochatRst.to_id();
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(toId);
        if (_userConnMap.end() != it)
        {
            // toid在线,在本服务器,转发消息 服务器主动推送消息给toId用户
            std::string onechatmsg;
            cmsg.SerializeToString(&onechatmsg);
            it->second->send(onechatmsg);
            return;
        }
    }

    //查询toid是否在线,是否在其他服务器
    if (_userModel.cacheQuery(toId) == "online" || _userModel.query(toId).getState() == "online")
    {
        std::string onechatmsg;
        ochatRst.SerializeToString(&onechatmsg);
        _redis.publish(toId, onechatmsg);
        return;
    }

    // toid不在现,存储离线消息
    std::string onechatmsg;
    ochatRst.SerializeToString(&onechatmsg);
    _offlineMsgModel.insert(toId, onechatmsg);
}

//添加好友业务
void ChatService::addFriendRequest(const TcpConnectionPtr &conn, chat_proto::ChatMessage &cmsg, Timestamp time)
{
    // int userid = js["id"].get<int>();
    // int friendid = js["friendid"].get<int>();
    chat_proto::AddFriendRequest addfRst;
    if (!addfRst.ParseFromString(cmsg.message_body()))
    {
        LOG_ERROR << "parse error in addfriend request service";
    }
    uint32_t userid = addfRst.user_id();
    uint32_t friendid = addfRst.friend_id();
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(friendid);
        if (_userConnMap.end() != it)
        {
            // toid在线,在本服务器,转发消息 服务器主动推送消息给toId用户
            std::string onechatmsg;
            cmsg.SerializeToString(&onechatmsg);
            it->second->send(onechatmsg);
            return;
        }
    }

    //查询toid是否在线,是否在其他服务器
    if (_userModel.cacheQuery(friendid) == "online" || _userModel.query(friendid).getState() == "online")
    {
        std::string onechatmsg;
        cmsg.SerializeToString(&onechatmsg);
        _redis.publish(friendid, onechatmsg);
        return;
    }

    // toid不在现,存储离线消息
    std::string onechatmsg;
    cmsg.SerializeToString(&onechatmsg);
    _offlineMsgModel.insert(friendid, onechatmsg);
}
void ChatService::addFriendResponse(const TcpConnectionPtr &conn, chat_proto::ChatMessage &cmsg, Timestamp time)
{
    chat_proto::AddFriendResponse addfRsp;
    if (!addfRsp.ParseFromString(cmsg.message_body()))
    {
        LOG_ERROR << "parse error in addfriend response service";
    }
    uint32_t userid = addfRsp.user_id();
    uint32_t friendid = addfRsp.friend_id();
    if (addfRsp.pass())
    {
        //存储好友信息
        _friendModel.insert(userid, friendid);
        _friendModel.insert(friendid, userid);
    }
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(userid);
        if (_userConnMap.end() != it)
        {
            // toid在线,在本服务器,转发消息 服务器主动推送消息给toId用户
            std::string onechatmsg;
            cmsg.SerializeToString(&onechatmsg);
            it->second->send(onechatmsg);
            return;
        }
    }

    //查询toid是否在线,是否在其他服务器
    if (_userModel.cacheQuery(userid) == "online" || _userModel.query(userid).getState() == "online")
    {
        std::string onechatmsg;
        cmsg.SerializeToString(&onechatmsg);
        _redis.publish(userid, onechatmsg);
        return;
    }

    // toid不在现,存储离线消息
    std::string onechatmsg;
    cmsg.SerializeToString(&onechatmsg);
    _offlineMsgModel.insert(userid, onechatmsg);
}

//创建群组业务
void ChatService::createGroup(const TcpConnectionPtr &conn, chat_proto::ChatMessage &cmsg, Timestamp time)
{
    chat_proto::CreateGroupRequest creategRst;
    if (!creategRst.ParseFromString(cmsg.message_body()))
    {
        LOG_ERROR << "parse error in creategroup request service";
    }
    // int userid = js["id"].get<int>();
    // string name = js["groupname"];
    // string desc = js["groupdesc"];

    //存储新创建的群组信息
    Group group(-1, creategRst.group_name(), creategRst.group_description());
    if (_groupModel.createGroup(group))
    {
        //存储群创建人信息
        _groupModel.addGroup(creategRst.user_id(), group.getId(), "creator");
    }
    chat_proto::CreateGroupResponse creategRsp;
    creategRsp.set_group_id(group.getId());
    creategRsp.set_error_status(chat_proto::CRE_GRP_ERR);

    chat_proto::ChatMessage chatmsg;
    chatmsg.set_type(chat_proto::CREATE_GROUP_ACK);
    std::string *chatmsgbody = chatmsg.mutable_message_body();
    creategRsp.SerializeToString(chatmsgbody);
    std::string creategmsg;
    chatmsg.SerializeToString(&creategmsg);
    // conn->send("create Group successfully!");
}

//加入群组业务
void ChatService::addGroupRequest(const TcpConnectionPtr &conn, chat_proto::ChatMessage &cmsg, Timestamp time)
{
    // int userid = js["id"].get<int>();
    // int groupid = js["groupid"].get<int>();
    chat_proto::AddGroupRequest addgRst;
    if (!addgRst.ParseFromString(cmsg.message_body()))
    {
        LOG_ERROR << "parse error in addgroup request service";
    }

    chat_proto::AddGroupResponse addgRsp;
    addgRsp.set_pass(true);
    if (addgRsp.pass())
    {
        _groupModel.addGroup(addgRst.user_id(), addgRst.group_id(), "normal");
    }
    std::string addgmsgbody;
    addgRsp.SerializeToString(&addgmsgbody);
    cmsg.set_message_body(addgmsgbody);
    std::string addgmsg;
    cmsg.SerializeToString(&addgmsg);
    conn->send(addgmsg);
    // conn->send("add Group successfully!");
}
void ChatService::addGroupResponse(const TcpConnectionPtr &conn, chat_proto::ChatMessage &cmsg, Timestamp time)
{
}

//群组聊天业务
void ChatService::groupChat(const TcpConnectionPtr &conn, chat_proto::ChatMessage &cmsg, Timestamp time)
{
    chat_proto::GroupChatRequest gcRst;
    if (!gcRst.ParseFromString(cmsg.message_body()))
    {
        LOG_ERROR << "parse error in groupchat request service";
    }
    // int userid = js["id"].get<int>();
    // int groupid = js["groupid"].get<int>();

    vector<int> useridVec = _groupModel.queryGroupUsers(gcRst.user_id(), gcRst.group_id());
    std::string gcmsg;
    cmsg.SerializeToString(&gcmsg);
    lock_guard<mutex> lock(_connMutex);
    for (const int &id : useridVec)
    {
        auto it = _userConnMap.find(id);
        if (it != _userConnMap.end())
        {
            //转发群消息
            it->second->send(gcmsg);
        }
        else
        {
            //查询toid是否在线
            if (_userModel.cacheQuery(id) == "online" || _userModel.query(id).getState() == "online")
            {
                _redis.publish(id, gcmsg);
            }
            else
            {
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
    if (it != _userConnMap.end())
    {
        it->second->send(msg);
        return;
    }

    //可能下线,存储该用户的离线消息
    _offlineMsgModel.insert(userid, msg);
}
