#ifndef GROUPMODEL_H
#define GROUPMODEL_H

#include <string>
#include <vector>
#include "others.pb.h"

class GroupModel
{
public:
    //创建群组
    bool createGroup(chat_proto::Group &group);
    //加入群组
    void addGroup(uint32_t userid, uint32_t groupid, std::string role);
    //查询用户所在群组信息
    std::vector<chat_proto::Group> queryGroups(uint32_t userid);
    //根据指定的groupid查询群组用户id列表,除userid自己,主要用户群聊业务给其他成员发送信息
    std::vector<uint32_t> queryGroupUsers(uint32_t userid, uint32_t groupid);
};

#endif