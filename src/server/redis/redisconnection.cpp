#include "redisconnection.hpp"

std::string RedisBase::_err_msg = "";

RedisConnection::RedisConnection()
{

}

RedisConnection::~RedisConnection()
{
    redisFree(pm_rct);
}

bool RedisConnection::connect(const std::string &addr, int port, const std::string &pwd,int db) {
    addr_ = addr;
    port_ = port;
    pwd_ = pwd;
    pm_rct = redisConnect(addr_.c_str(), port_);
    if (pm_rct->err){
        _is_connect = false;
    }
    if (!pwd_.empty()){
        connectAuth(pwd_);
    }
    selectDb(db);

    _is_connect = true;
    return _is_connect;
}

/*
使用密码登录
psw：登录密码
成功返回M_REDIS_OK，失败返回<0
*/
int RedisConnection::connectAuth(const std::string &psw)
{
    std::string cmd = "auth " + psw;
    pm_rr = (redisReply*)redisCommand(pm_rct, cmd.c_str());
    return 0;
}


int RedisConnection::selectDb(const int &db)
{
    std::string cmd = "select " + std::to_string(db);
    pm_rr = (redisReply*)redisCommand(pm_rct, cmd.c_str());
    return 0;
}

bool RedisConnection::isConnect()
{
    return _is_connect;
}

RedisStatus RedisConnection::checkError(redisReply *reply)
{
    if(reply == nullptr){
        reply = pm_rr;
    }
    if (pm_rct->err){
        RedisBase::setError(pm_rct->errstr);
        return M_CONTEXT_ERROR;
    }
    if (reply == nullptr){
        RedisBase::setError("auth redisReply is nullptr");
        return M_REPLY_ERROR;
    }
    return M_REDIS_OK;
}

RedisStatus RedisConnection::parseReplay(RedisResult &result,redisReply *reply)
{
    if(reply == nullptr){
        reply = pm_rr;
    }
    RedisStatus s = checkError(reply);
    if(s != M_REDIS_OK){
        freeRedisReply(reply);
        return s;
    }

    switch(reply->type)
    {
        case REDIS_REPLY_STATUS:
            s = M_REDIS_OK;
            result.type = reply->type;
            result.strdata = reply->str;
            break;
        case REDIS_REPLY_ERROR:
            s = M_REPLY_ERROR;
            // result.type = redis_reply_null;
            result.type = reply->type;
            result.strdata =reply->str;
            RedisBase::setError(reply->str);
            break;
        case REDIS_REPLY_STRING:
            s = M_REDIS_OK;
            // result.type = redis_reply_string;
            result.type = reply->type;
            result.strdata = reply->str;
            break;
        case REDIS_REPLY_INTEGER:
            s = M_REDIS_OK;
            // result.type = redis_reply_integer;
            result.type = reply->type;
            result.inter = reply->integer;
            break;
        case REDIS_REPLY_ARRAY:
            s = M_REDIS_OK;
            // result.type = redis_reply_array;
            result.type = reply->type;
            for (int i = 0; i < reply->elements; i ++){
                if(reply->element[i]->type == REDIS_REPLY_NIL){
                    // result.vecdata.push_back(NIL);
                    result.vecdata.emplace_back("nil");
                }
                else{
                    if(reply->element[i]->str == nullptr){
                        result.vecdata.emplace_back("null");
                    }
                    else{
                        result.vecdata.emplace_back(reply->element[i]->str);
                    }
                }
            }
            break;
        case REDIS_REPLY_NIL:
            s = M_NIL_ERROR;
            // result.type = redis_reply_null;
            result.type = reply->type;
            result.strdata = "REDIS_REPLY_NIL";
            if(reply->str == nullptr){
                RedisBase::setError("REDIS_REPLY_NIL");
            }
            else{
                RedisBase::setError(reply->str);
            }
            break;
        default:
            s = M_REPLY_ERROR;
            // result.type = redis_reply_invalid;
            result.type = reply->type;
            break;
    }

    freeRedisReply(reply);
    return s;
}

void RedisConnection::freeRedisReply(redisReply *reply)
{
    if (reply){
        freeReplyObject(reply);
        reply = nullptr;
    }
}

RedisStatus RedisConnection::execCommand(const std::string &cmd, RedisResult &res)
{
    pm_rr = static_cast<redisReply *>(redisCommand(pm_rct, cmd.c_str()));
    return this->parseReplay(res, pm_rr);
}

