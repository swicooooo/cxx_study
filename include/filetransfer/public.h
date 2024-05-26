#pragma once

#include <string.h>
#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h>


// 全局检查错误
#define CheckErr(ret, str) \
    do \
    {  \
        if (ret < 0)    \
        { \
            printf("%s:%d error message is: %s\n", __FILE__, __LINE__, str);  \
            exit(-1);    \
        } \
    } while(0);

static const int BLOCK_SIZE = 2048;     // 分块的大小

// 操作类型
enum OperType
{
    TYPE_DOWNLOAD = 1,
    TYPE_UPLOAD,
    TYPE_ACK,
    TYPE_AUTH,
};

// 文件类型
enum FileType
{
    TYPE_NORMAL,    // 普通传输
    TYPE_QUICK,     // 秒传
    TYPE_EXIST,     // 文件已经存在
};

// 结构体和字符数组可以强制转换
// struct存储的成员数据是连续的，但是会根据字节对齐而改变总大小
struct DataPacket
{
    int datalen;            // 弹性数组数据长度
    enum OperType type;     // 当前数据包代表的操作类型 
    char data[0];           // 存储可变TransferFile数据
};

// 上传文件
struct UploadFile
{
    char filepath[128];     // 文件路径
    uint64_t filesize;      // 文件大小
    char md5[33];           // 32位的md5值
};

// 文件上传响应
struct UploadFileAck
{
    enum FileType type;
    int blockno;
};

// 传输文件
struct TransferFile
{
    int blockno;
    char content[BLOCK_SIZE];
};

// 传输文件响应
struct TransferFileAck
{
    char flag;
};