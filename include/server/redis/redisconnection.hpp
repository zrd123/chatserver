#include <vector>
#include <string>
#include <hiredis/hiredis.h>

//redis 的执行状态
enum RedisStatus
{
    M_REDIS_OK = 0, //执行成功
    M_CONNECT_FAIL = -1, //连接redis失败
    M_CONTEXT_ERROR = -2, //RedisContext返回错误
    M_REPLY_ERROR = -3, //redisReply错误
    M_EXE_COMMAND_ERROR = -4, //redis命令执行错误
    M_NIL_ERROR = -5 //nil
};

struct RedisResult
{
    int type;
    size_t inter;
    std::string strdata;
    std::vector<std::string> vecdata;
};

class RedisBase
{
public:
    RedisBase();
    ~RedisBase();

    virtual std::string getError(){
        return _err_msg;
    }

    static void setError(std::string error){
        _err_msg = error;
    }

private:
    static std::string _err_msg;
};

class RedisConnection
{
public:
    RedisConnection();
    ~RedisConnection();
    /**
     * 连接服务
     * @param addr  地址
     * @param port  端口
     * @param pwd   密码
     * @param db    数据库
     * @return      是否成功
     */
    bool connect(const std::string &addr, int port, const std::string &pwd = "",int db=0);
    
    /**
     * 是否连接
     * @return 
     */
    bool isConnect();
    
    /**
     * 解析结果
     * @param result 
     * @param reply 
     * @return 
     */
    RedisStatus parseReplay(RedisResult &result, redisReply *reply = nullptr);
    
    /**
     * 执行命令
     * @param cmd 
     * @param res 
     * @return 
     */
    RedisStatus execCommand(const std::string &cmd, RedisResult &res);

    RedisStatus ssSet(int groupid, const std::string username, RedisResult &res);
private:
    /**
     * 使用密码
     * @param psw 
     * @return 
     */
    int connectAuth(const std::string &psw);

    /**
     * 释放命令执行结果的内存
     * @param reply 
     * @return 
     */
    void freeRedisReply(redisReply *reply);
    
    /**
     * 解析错误
     * @param reply 
     * @return 
     */
    RedisStatus checkError(redisReply *reply = NULL);
    
    /**
     * 选择数据库
     * @param db 
     * @return 
     */
    int selectDb(const int &db);

public:

    redisContext* pm_rct; //redis结构体
    redisReply* pm_rr; //返回结构体

    std::string addr_; //IP地址
    int port_; //端口号
    std::string pwd_; //密码

private:
    bool _is_connect = false;
};