#include "usermodel.hpp"
#include "db.h"
#include "redisconnectionpool.hpp"
#include <iostream>

//User表的增加方法
bool UserModel::insert(User &user)
{
    char sql[1024] = {0};
    sprintf(sql, "insert into user(name, password, state) values('%s', '%s', '%s')",
        user.getName().c_str(), user.getPwd().c_str(), user.getState().c_str());
    MySQL mysql;
    if(mysql.connect()){
        if(mysql.update(sql)){
            //获取插入成功的用户数据生成的主键ID
            user.setId(mysql_insert_id(mysql.getConnection()));
            return true;
        }
    }
    return false;
}

//根据用户哈号码查询用户信息
User UserModel::query(unsigned int id)
{
    char sql[1024] = {0};
    sprintf(sql, "select * from user where id = %d", id);
    MySQL mysql;
    if(mysql.connect()){
        MYSQL_RES *res = mysql.query(sql);
        if(nullptr != res){
            MYSQL_ROW row = mysql_fetch_row(res);
            if(nullptr != row){
                User user;
                user.setId(atoi(row[0]));
                user.setName(row[1]);
                user.setPwd(row[2]);
                user.setState(row[3]);

                auto redis = RedisConnectionPool::Get();
                RedisResult result;
                sprintf(sql, "SET %d %s", user.getId(), user.getState());
                redis.get()->execCommand(sql, result);
                RedisConnectionPool::Back(redis);

                mysql_free_result_nonblocking(res);
                return user;
            }
        }
    }
    return User();
}

std::string UserModel::cacheQuery(unsigned int id)
{
    char sql[1024];
    auto redis = RedisConnectionPool::Get();
    RedisResult result;
    sprintf(sql, "GET %d", id);
    redis.get()->execCommand(sql, result);
    RedisConnectionPool::Back(redis);
    return result.strdata;
}


//更新用户状态信息
bool UserModel::updateState(User user)
{
    char sql[1024] = {0};
    sprintf(sql, "update user set state = '%s' where id = '%d'", user.getState().c_str(), user.getId());
    MySQL mysql;
    if(mysql.connect()){
        auto redis = RedisConnectionPool::Get();
        RedisResult result;
        if(mysql.update(sql)){
            sprintf(sql, "SET %d %s", user.getId(), user.getState().c_str());
            redis.get()->execCommand(sql, result);
            RedisConnectionPool::Back(redis);
            return true;
        }
        RedisConnectionPool::Back(redis);
    }
    return false;
}

//重置用户状态信息
void UserModel::resetState()
{
    char sql[1024] = "update user set state = 'offline' where state = 'online'";
    MySQL mysql;
    if(mysql.connect()){
        mysql.update(sql);
        // auto redis = RedisConnectionPool::Get();
        // RedisResult result;
        // sprintf(sql, "flushall");
        // redis.get()->execCommand(sql, result);
        // RedisConnectionPool::Back(redis);
    }
}