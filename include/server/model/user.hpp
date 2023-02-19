#ifndef USER_H
#define USER_H

#include <string>
using string = std::string;

//匹配User表的ORM类
class User
{
public:
    User(int id = -1, string name = "", string pwd = "", string state = "offline")
    {
        this->id = id;
        this->name = name;
        this->password = pwd;
        this->state = state;
    }

    void setId(int id) { this->id = id; }
    void setName(const string name) { this->name = name; }
    void setPwd(const string pwd) { this->password = pwd; }
    void setState(const string state) { this->state = state; }

    uint32_t getId() const { return this->id; }
    const string getName() const { return this->name; }
    const string getPwd() const { return this->password; }
    const string getState() const { return this->state; }
private:
    int id;
    string name;
    string password;
    string state;
};

#endif