#include "offlinemessagemodel.hpp"
#include "db.h"
#include <iostream>

//存储用户的离线消息
void OfflineMsgModel::insert(uint32_t userid, std::string &msg)
{
    char sql[1024] = {0};
    sprintf(sql, "insert into offlinemessage values(%d, '%s')", userid, msg.c_str());
    std::cout << msg << std::endl;

    MySQL mysql;
    if(mysql.connect()){
        mysql.update(sql);
    }
}

//删除用户离线消息
void OfflineMsgModel::remove(uint32_t userid)
{
    char sql[1024] = {0};
    sprintf(sql, "delete from offlinemessage where userid=%d", userid);

    MySQL mysql;
    if(mysql.connect()){
        mysql.update(sql);
    }
}

//查询用户的离线消息
std::vector<std::string> OfflineMsgModel::query(uint32_t userid)
{
    char sql[1024] = {0};
    sprintf(sql, "select message from offlinemessage where userid= %d", userid);

    std::vector<std::string> vec;
    MySQL mysql;
    if(mysql.connect()){
        MYSQL_RES *res = mysql.query(sql);
        if(nullptr != res){
            //把userid用户的离线消息放入vec中返回
            MYSQL_ROW row; 
            while((row = mysql_fetch_row(res)) != nullptr){ 
                vec.push_back(row[0]);
            }
            mysql_free_result(res);
            return vec;
        }
    }
    return vec;
}