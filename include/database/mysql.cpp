#include "mysql.h"

#include <mymuduo/Logger.h>

static std::string server = "127.0.0.1";
static std::string user = "root";
static std::string password = "0";
static std::string dbname = "chat";

MySQL::MySQL()
{
    conn_ = mysql_init(nullptr);    // nor not mysql_init(conn_)
}

MySQL::~MySQL()
{
    if(conn_ != nullptr) 
    {
        mysql_close(conn_);
    }
}

bool MySQL::connect()
{
    MYSQL *p = mysql_real_connect(conn_,server.c_str(),user.c_str(),password.c_str(),dbname.c_str(),3306,nullptr,0);
    if(p == nullptr) 
    {
        LOG_ERROR("%s:%d: mysql connect error",__FILE__,__LINE__);
    }
    return p;
}

bool MySQL::update(std::string sql)
{
    if(mysql_query(conn_, sql.c_str())) 
    {
        LOG_ERROR("%s:%d:%s update error",__FILE__,__LINE__,sql.c_str());
        return false;
    }
    return true;
}

MYSQL_RES *MySQL::query(std::string sql)
{
    if(mysql_query(conn_, sql.c_str())) 
    {
        LOG_ERROR("%s:%d:%s query error",__FILE__,__LINE__,sql.c_str());
        return nullptr;
    }
    return mysql_use_result(conn_);
}

MYSQL* MySQL::getConn()
{
    return conn_;
}