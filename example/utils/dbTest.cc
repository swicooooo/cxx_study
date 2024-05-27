#include "database/mysql.h"

#include <memory>

void dbTest()
{
    std::unique_ptr<MySQL> m(new MySQL("file"));
    m->connect();
    m->update("insert into file(name, path, md5, state, blockno) values('hello.txt','./','md5-sim','0',0);");
    m->query("delete from file where name = 'hello.txt';");
}

int main(int argc, char const *argv[])
{
    return 0;
}
