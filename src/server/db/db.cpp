#include "db.h"

static std::string server = "127.0.0.1";
static std::string user = "root";
static std::string password = "mysql";
static std::string dbname = "chat";

// 申请内存
MySQL::MySQL()
{
    _conn = mysql_init(nullptr);
}
//释放内存
MySQL::~MySQL()
{
    if(nullptr != _conn){
        mysql_close(_conn);
    }
}
// 连接数据库
bool MySQL::connect()
{
    MYSQL *p = mysql_real_connect(_conn, server.c_str(), user.c_str(),
        password.c_str(), dbname.c_str(), 3306, nullptr, 0);
    if(nullptr != p){
        //默认编码是ASCII
        mysql_query(_conn, "set names gbk");
        LOG_INFO << "connect mysql successfully!";
    }
    else{
        LOG_INFO << "connection myqsl failed!";
    }
    return p;
}
// 更新操作
bool MySQL::update(std::string sql)
{
    if(mysql_query(_conn, sql.c_str())){
        LOG_INFO << __FILE__ << ":" << __LINE__ << ":" << sql << "update failed!";
        return false;
    }
    return true;
}
// 查询操作
MYSQL_RES *MySQL::query(std::string sql)
{
    if(mysql_query(_conn, sql.c_str())){
        LOG_INFO << __FILE__ << ":" << __LINE__ << ":" << sql << "query failed!";
        return nullptr;
    }
    return mysql_use_result(_conn);
}

//获取连接
MYSQL *MySQL::getConnection()
{
    return _conn;
}
