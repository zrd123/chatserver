#ifndef USERMODEL_H
#define USERMODEL_H

#include "user.hpp"

//User表的数据操作类
class UserModel
{
public:
    //User表的增加方法
    bool insert(User &user);

    //根据用户哈号码查询用户信息
    User query(uint32_t id);

    //从Redis查询用户的状态信息
    std::string cacheQuery(uint32_t id);

    //更新用户状态信息
    bool updateState(User user);

    //重置用户状态信息
    void resetState();
};

#endif