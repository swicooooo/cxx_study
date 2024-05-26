#pragma once

#include <string>
#include <mysql/mysql.h>

class MySQL
{
public:
    MySQL(std::string server = "127.0.0.1", std::string user = "root", std::string password = "0", std::string dbname);
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