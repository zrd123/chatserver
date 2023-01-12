#ifndef DB_H
#define DB_H

#include <string>
#include <mysql/mysql.h>
#include <muduo/base/Logging.h>

class MySQL
{
public:
    //申请内存
    MySQL();
    //释放内存
    ~MySQL();
    //连接数据库
    bool connect();
    //更新操作
    bool update(std::string sql);
    //查询操作
    MYSQL_RES *query(std::string sql);
    //获取链接
    MYSQL *getConnection();
private:
    MYSQL *_conn;
};



#endif