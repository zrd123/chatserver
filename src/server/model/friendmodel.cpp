#include "friendmodel.hpp"
#include "db.h"
#include "others.pb.h"

//添加好友关系
void FriendModel::insert(int userid, int friendid)
{
    char sql[1024];
    sprintf(sql, "insert into friend values(%d, %d)", userid, friendid);

    MySQL mysql;
    if(mysql.connect()){
        mysql.update(sql);
    }
}

//返回用户好友列表
vector<chat_proto::User> FriendModel::query(int userid)
{
    char sql[1024] = {0};
    sprintf(sql, "select a.id,a.name,a.state from user a inner join friend b on b.friendid = a.id where b.userid=%d", userid);

    vector<chat_proto::User> vec;
    // vector<User> vec;
    MySQL mysql;
    if(mysql.connect()){
        MYSQL_RES *res = mysql.query(sql);
        if(nullptr != res){
            //把userid用户的所有好友放入vec中返回
            MYSQL_ROW row;
            while((row = mysql_fetch_row(res)) != nullptr){
                // User user;
                // user.setId(atoi(row[0]));
                // user.setName(row[1]);
                // user.setState(row[2]);
                chat_proto::User user;
                user.set_id(atoi(row[0]));
                user.set_name(row[1]);
                user.set_status(row[2]);
                vec.push_back(user);
            }
            mysql_free_result_nonblocking(res);
            return vec;
        }
    }
    return vec;
}