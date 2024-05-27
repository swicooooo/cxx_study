#pragma once

#include "database/mysql.h"
#include "public.h"

#include <vector>
#include <memory>

/**
 * mysql tb：file
    CREATE TABLE file (		\
        CREATE TABLE file (	\
        name VARCHAR(255) NOT NULL COMMENT "file name",	\
        path VARCHAR(255) NOT NULL COMMENT "dir path", 		\
        md5 VARCHAR(32) NOT NULL COMMENT "file encrypt",	\
        state VARCHAR(1) NOT NULL COMMENT "file state 0: incomplete,1: complete",	\
        blockno INTEGER NOT NULL COMMENT "the next block",	\
        PRIMARY KEY (name, path)		\
    )ENGINE=InnoDB;
 * );
*/

class filetransfer
{
public:
    // 通用网络包传递函数
    // 封包的基本思想：通过将各类结构体强转为void*通用指针后存入DataPacket的data字符数组成员中
    // 解包的基本思想：当解包时直接强转为对应结构体即可，字符数组和结构体可相互强转
    int sendPacket(int sockfd, enum OperType t, void *data, int datalen);
    int recvPacket(int sockfd, void *buf, int size);
};

class filetransferServer: public filetransfer
{
public:
    filetransferServer();
    ~filetransferServer();
    void processUpload(int clientfd, struct DataPacket *dp);    // 处理文件上传函数
private:
    std::vector<std::string> queryFileInfo(const char *sql);
    bool instructClientSendBlock(int clientfd, const std::vector<std::string> &vec,  const struct UploadFile &uploadFile);
    void recvClientSendBlock(uint64_t &recvsize);
    void updateFileState(uint64_t &recvsize, const std::vector<std::string> &vec);
    MySQL *mysql_;                      // mysql处理句柄
    int ret_;                           // 通用linux服务调用返回值
    int clientfd_;                      // 通信的客户端fd
    std::string filePath_;              // 完整文件路径
    std::string path_;                  // 文件路径(不包含文件名)
    std::string name_;                  // 文件名
    uint64_t filesize_;                 // 文件大小
    std::string md5_;                   // 文件的md5
    int blocknum_;                      // 文件的块号
};

class filetransferClient: public filetransfer
{
public:
    filetransferClient() = default;
    ~filetransferClient() = default;
    void uploadFile(int sockfd, const std::string &path);   // 处理上传文件函数
private:
    uint64_t getFileSize(const std::string &path);
    void sendBlockByType(const struct UploadFileAck &uploadfileack, int totalblock, const std::string &path);
    int ret_;                // 通用linux服务调用返回值
    int sockfd_;             //  客户端与服务端通信sockfd
};



