#include "groupmodel.hpp"
#include "db.h"

//创建群组
bool GroupModel::createGroup(chat_proto::Group &group)
{
    char sql[1024] = {0};
    sprintf(sql, "insert into allgroup(groupname, groupdesc) values('%s', '%s')",
            group.name().c_str(), group.description().c_str());
    
    MySQL mysql;
    if(mysql.connect()){
        if(mysql.update(sql)){
            group.set_id(mysql_insert_id(mysql.getConnection()));
            return true;
        }
    }
    return false;
}

//加入群组
void GroupModel::addGroup(uint32_t userid, uint32_t groupid, std::string role)
{
    char sql[1024] = {0};
    sprintf(sql, "insert into groupuser values(%d, %d, '%s')",
            groupid, userid, role.c_str());
    
    MySQL mysql;
    if(mysql.connect()){
        mysql.update(sql);
    }
}

//查询用户所在群组信息
std::vector<chat_proto::Group> GroupModel::queryGroups(uint32_t userid)
{
    /*
    先根据userid在groupuser表中查询出该用户所属的群组信息
    再根据群组信息,查询属于该群组的所有用户的userid,并且和user表进行多表联合查询,查出用户的详细信息
    */
    char sql[1024] = {0};
    sprintf(sql, "select a.id,a.groupname,a.groupdesc from allgroup a inner join \
            groupuser b on a.id = b.groupid where b.userid = %d", userid);
    
    std::vector<chat_proto::Group> groupVec;
    MySQL mysql;
    if(mysql.connect()){
        MYSQL_RES *res = mysql.query(sql);
        if(nullptr != res){
            MYSQL_ROW row;
            //查出userid所有的群组信息
            while((row = mysql_fetch_row(res)) != nullptr){
                chat_proto::Group group;
                group.set_id(atoi(row[0]));
                group.set_name(row[1]);
                group.set_description(row[2]);
                // group.setId(atoi(row[0]));
                // group.setName(row[1]);
                // group.setDesc(row[2]);
                groupVec.push_back(group);
            }
            mysql_free_result_nonblocking(res);
        }
    }
    //查询群组的用户信息
    for(chat_proto::Group &group : groupVec){
        sprintf(sql, "select a.id,a.name,a.state,b.grouprole from user a \
        inner join groupuser b on b.userid = a.id where b.groupid = %d", group.id());
        MYSQL_RES *res = mysql.query(sql);
        if(nullptr != res){
            MYSQL_ROW row;
            while((row = mysql_fetch_row(res)) != nullptr){
                chat_proto::GroupMember *member = group.add_members();
                chat_proto::User *user = member->mutable_user();
                user->set_id(atoi(row[0]));
                user->set_name(row[1]);
                user->set_status(row[2]);
                member->set_role(row[3]);
            }
            mysql_free_result_nonblocking(res);
        }
    }
    return groupVec;
}

//根据指定的groupid查询群组用户id列表,除userid自己,主要用户群聊业务给其他成员发送信息
std::vector<uint32_t> GroupModel::queryGroupUsers(uint32_t userid, uint32_t groupid)
{
    char sql[1024] = {0};
    sprintf(sql, "select userid from groupuser where groupid = %d and userid != %d", groupid, userid);

    std::vector<uint32_t> idVec;
    MySQL mysql;
    if(mysql.connect()){
        MYSQL_RES *res = mysql.query(sql);
        if(nullptr != res){
            MYSQL_ROW row;
            while((row = mysql_fetch_row(res)) != nullptr){
                idVec.push_back(atoi(row[0]));
            }
            mysql_free_result_nonblocking(res);
        }
    }
    return idVec;
}
