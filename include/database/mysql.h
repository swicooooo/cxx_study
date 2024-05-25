#pragma once

#include <string>
#include <mysql/mysql.h>

class MySQL
{
public:
    MySQL();
    ~MySQL();
    bool connect();   
    bool update(std::string sql);
    MYSQL_RES* query(std::string sql);
    MYSQL* getConn();
private:
    MYSQL *conn_;
};