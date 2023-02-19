#ifndef GROUP_H
#define GROUP_H

#include "groupuser.hpp"
#include <string>
#include <vector>

using std::string;
using std::vector;

//User表的ORM类
class Group
{
public:
    Group(int id = -1, string name = "", string desc = "")
    {
        this->id = id;
        this->name = name;
        this->desc = desc;
    }

    void setId(int id) { this->id = id; }
    void setName(const string name) { this->name = name; }
    void setDesc(const string desc) { this->desc = desc; }

    int getId() const { return this->id; }
    const string getName() const { return this->name; }
    const string getDesc() const { return this->desc; }
    vector<GroupUser> &getUsers(){ return this->users; }

private:
    int id;
    string name;
    string desc;
    vector<GroupUser> users;
};

#endif