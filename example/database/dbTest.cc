#include "database/mysql.h"

#include <memory>

int main(int argc, char const *argv[])
{
    std::unique_ptr<MySQL> m(new MySQL("file"));
    m->connect();
    m->update("insert into file(name, path, md5, state, blockno) values('hello.txt','./','md5-sim','0',0);");
    m->query("delete from file where name = 'hello.txt';");
    return 0;
}
