#include "filetransfer/filetransfer.h"

#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <signal.h>
#include <thread>

bool isFileServerRunning = true;

void SigIntHandle(int)
{
    isFileServerRunning = false;
    exit(-1);
}

int main(int argc, char const *argv[])
{
    signal(SIGINT, SigIntHandle);   // 注册ctrl+c信号

    int ret;
    int sockfd =socket(AF_INET, SOCK_STREAM, 0);

    std::string ip = "127.0.0.1";
    uint16_t port = 8000;
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip.c_str());
    
    ret = bind(sockfd, (struct sockaddr*)&addr, sizeof(addr));
    CheckErr(ret, "bind err"); 
    ret = listen(sockfd, 1024);
    CheckErr(ret, "listen err");

    struct sockaddr_in clientaddr;
    socklen_t clen = sizeof(clientaddr);
    while (isFileServerRunning)
    {
        // 处理客户端连接请求
        int clientfd = accept(sockfd, (sockaddr*)&clientaddr, &clen);
        if(clientfd < 0)
        {
            CheckErr(clientfd, "accept err");
        }
        char buffer[1024] = {0};
        // 开启一个线程处理通信
        std::thread([&](){
            while (1)
            {
                // 接受客户端发来的请求
                bzero(buffer,1024);
                ret = recv(clientfd, buffer, 1024, 0);
                CheckErr(ret, "recv err");

                // 对端关闭
                if(ret == 0)
                {
                    close(clientfd);
                    break;
                }
                
                // 强转数据包
                struct DataPacket *dp = (struct DataPacket*)buffer;
                filetransferServer s;
                switch (dp->type)
                {
                case TYPE_UPLOAD:
                    s.processUpload(clientfd, dp);
                    break;
                
                default:
                    break;
                }
            }
        }).detach();
    }
    close(sockfd);
    printf("fileserver quit!\n");
    return 0;
}
