#include "filetransfer/filetransfer.h"

#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

int main(int argc, char const *argv[])
{
    int ret;
    int sockfd =socket(AF_INET, SOCK_STREAM, 0);

    std::string ip = "127.0.0.1";
    uint16_t port = 8000;
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip.c_str());

    ret = connect(sockfd, (struct sockaddr*)&addr, sizeof(addr));
    CheckErr(ret, "connect err");
    printf("connect fileserver success!\n");

    filetransferClient client;
    client.uploadFile(sockfd, "./doc/hello.txt");

    return 0;
}
