#pragma once

#include <string>
#include <mysql/mysql.h>

// 用代码访问mysql会出现无法访问root用户，直接创建新用户即可
class MySQL
{
public:
    MySQL(std::string dbname, std::string server = "127.0.0.1", std::string user = "sw", std::string password = "0");
    ~MySQL();
    bool connect();   
    bool update(std::string sql);
    MYSQL_RES* query(std::string sql);
    MYSQL* getConn();
private:
    MYSQL *conn_;
    std::string server_;
    std::string user_;
    std::string password_;
    std::string dbname_;
};