#ifndef FRIENDMODEL_H
#define FRIENDMODEL_H

#include <vector>
#include "user.hpp"
#include "others.pb.h"
using std::vector;

//维护好友信息的操作类
class FriendModel
{
public:
    //添加好友关系
    void insert(int userid, int friendid);

    //返回用户好友列表
    vector<chat_proto::User> query(int userid);
};

#endif