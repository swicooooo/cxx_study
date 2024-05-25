#include "database/mysql.h"
#include "public.h"

#include <string>
#include <vector>
#include <unistd.h>
#include <stdint.h>
#include <sys/socket.h>
#include <openssl/md5.h>

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

// 通用网络包打包发送函数
int sendPacket(int sockfd, enum OperType t, void *data, int datalen)
{
    // 构建数据包发送给客户端
    struct DataPacket *dp = (struct DataPacket*)data;
    dp->datalen = datalen;
    dp->type = t;

    if(data != NULL)
    {
        memcpy(dp->data, data, datalen);
    }
    int ret = send(sockfd, dp, sizeof(struct DataPacket)+datalen, 0);
    free(dp);
    return ret;
}

// 通用网络包接收函数
int recvPacket(int sockfd, void *buf, int size)
{
    bzero(buf, size);
    return recv(sockfd, buf, size, 0);
}

// 处理上传功能
void processUpload(int clientfd, struct DataPacket *dp)
{
    struct UploadFile uploadFile;
    memcpy(&uploadFile, dp->data, dp->datalen);
    
    // "select state,md5,blockno from file where name='%s' and path='%s'"
    /**
     * 1. path+name 不存在  普传
     * 2. path+name 存在
     *          state == '1' && md5 == md5  秒传
     *          state == '0'                断点续传
     * */    

    // 判定文件路径是否合法
    int ret;
    std::string filePath = uploadFile.filepath;
    int idx = filePath.find_last_of("/");
    if(idx == std::string::npos)
    {
        std::string response = "illegal file path!" + filePath;
        ret = sendPacket(clientfd, TYPE_ACK, (void*)response.c_str(), response.length());
        CheckErr(ret, "sendPacket err!");
        return;
    }

    // 查询数据库关于文件的信息
    std::string path = filePath.substr(0, idx+1);
    std::string name = filePath.substr(idx+1);
    uint64_t filesize = uploadFile.filesize;
    std::string md5 = uploadFile.md5;
    MySQL mysql;
    char sql[1024] = {0};
    snprintf(sql, 1024, "select state,md5,blockno from file where name='%s' and path='%s'", path.c_str(), path.c_str());
    MYSQL_RES *res = mysql.query(sql);
    std::vector<std::string> vec{};
    if(res != nullptr) {
        MYSQL_ROW row = mysql_fetch_row(res);
        if(row != nullptr)  {
            vec.push_back(row[0]);  // state
            vec.push_back(row[1]);  // md5
            vec.push_back(row[2]);  // blockno
            mysql_free_result(res);
        }
    }

    if(vec.empty())
    {
        // 普传 blockno：0
        struct UploadFileAck ack;
        ack.type = TYPE_NORMAL;
        ack.blockno = 0;
        ret = sendPacket(clientfd, TYPE_ACK, (void*)&ack, sizeof(ack));
        CheckErr(ret, "sendPacket err!");
    }
    else 
    {
        printf("state: %s, md5: %s", vec[0].c_str(), vec[1].c_str());
        if(vec[0] == "1" && vec[1] == uploadFile.md5)
        {
            // 秒传
            struct UploadFileAck ack;
            ack.type = TYPE_QUICK;
            ret = sendPacket(clientfd, TYPE_ACK, (void*)&ack, sizeof(ack));
            CheckErr(ret, "sendPacket err!");
        }
        else if(vec[0] == "1" && vec[1] != uploadFile.md5)
        {
            // 普传，名称一致但是不同文件
            struct UploadFileAck ack;
            ack.type = TYPE_NORMAL;
            ack.blockno = 1;    // TODO 在客户端选择上传的blocknum时会减1，这里是为了避免为-1
            ret = sendPacket(clientfd, TYPE_ACK, (void*)&ack, sizeof(ack));
            CheckErr(ret, "sendPacket err!");
            name += "-md5";
        }
        else
        {
            // 断点续传, 不完整数据
            struct UploadFileAck ack;
            ack.type = TYPE_NORMAL;
            ack.blockno = atoi(vec[2].c_str())+1;
            ret = sendPacket(clientfd, TYPE_ACK, (void*)&ack, sizeof(ack));
            CheckErr(ret, "sendPacket err!");
        }
    }

    // 通知完客户端，开始接收文件
    const int size = sizeof(DataPacket) + sizeof(TransferFile);
    char recvbuf[size] = {0};
    int blocknum = 0;
    uint64_t recvsize = 0;
    double progress = 0.0;

    // 如果能访问则flag为追加，否则为创建
    int localfd = access((path + name).c_str(), F_OK)==0?
                open((path+name).c_str(), O_APPEND | O_RDWR):
                open((path+name).c_str(), O_CREAT | O_RDWR | O_APPEND);
	CheckErr(localfd, "open err");

    // 开始循环接收packet
    while (1)
    {
        ret = recvPacket(clientfd, recvbuf, size);
        CheckErr(ret, "recvPacket err");
        if(ret == 0)
        {
            break;  // 客户端断开
        }

        // 将客户端发来的包进行转换
        DataPacket *dp = (DataPacket*)recvbuf;
        TransferFile *tfile = (TransferFile*)dp->data;
        int writesize = dp->datalen - sizeof(int);
        ret = write(localfd, dp->data, writesize);
        CheckErr(ret, "write err");

        // 记录, 返回ack带进度
        blocknum++;
        recvsize += writesize;
        char progbuf[128] = {0};
        snprintf(progbuf, 128, "progress is: %d%%", recvsize / filesize * 100);
        ret = sendPacket(clientfd, TYPE_ACK, (void*)progbuf, 128);
        CheckErr(ret, "sendPacket err");

        // 接收完成
        printf("recvsize: %d, filesize: %d", recvsize, filesize);
        if(recvsize == filesize)
        {
            break;
        }
    }
    close(localfd);
    
    // 在数据库中记录传输信息 blocknum
    // update user set blockno=blocknum  where name='%s' and path='%s'",name.c_str(), path.c_str());
    // 完全接收，校验完md5后向客户端发送ack提示
    if(recvsize == filesize)
    {
        // 使用openssl的MD5加密与客户端发来的MD5对比
        unsigned char newmd5[16] = {0};
        MD5((const unsigned char*)(path + name).c_str(), (path + name).length(), newmd5);
        if((memcmp(newmd5, md5.c_str(), 16)) == 0)
        {
            // MD5相同，响应ack文件传输成功
            // 如果数据库存在数据则更新数据，否则插入新数据
            if(vec.empty())
            {
                snprintf(sql, 1024, "insert into file(name, path, md5, state, blockno) values('%s', '%s', '%s', '%s', %d)",
					name.c_str(), path.c_str(), md5.c_str(), "1", blocknum);
            }
            else
            {
                snprintf(sql, 1024, "update file set state='1',blockno=%d where name='%s' and path='%s'",
					blocknum, name.c_str(), path.c_str());
            }
            mysql.update(sql);
            ret = sendPacket(clientfd, TYPE_ACK, (void*)"ok", strlen("ok"));
            CheckErr(ret, "SendPacket err");
        }
        else
        {
            // MD5不相同，响应ack文件传输失败，删除本地文件
            ret = sendPacket(clientfd, TYPE_ACK, (void*)"fail", strlen("fail"));
            CheckErr(ret, "SendPacket err");
        }
    }
    else
    {
        // 不完全接收，因为clientfd中断提前跳出
		if (vec.empty())
		{
			snprintf(sql, 1024, "insert into file values(name, path, md5, state, blockno) values('%s', '%s', '%s', '%s', %d)",
					name.c_str(), path.c_str(), md5.c_str(), "0", blocknum);
		}
        else
		{   
            // 如果存在更新blockno(ps:state="0")
			snprintf(sql, 1024, "update file set blockno=%d where name='%s' and path='%s'"
				,blocknum, name.c_str(), path.c_str());
		}
        mysql.update(sql);
    }
}

// /home/sw/filename
void upload_handler(int sockfd, const std::string &path)
{
    int ret;
    // 获取文件大小, 直接偏移fd的位置
    FILE *fp = fopen(path.c_str(), "r");
    CheckErr(fp, "fopen err");
    fseek(fp, 0, SEEK_END);
    uint64_t filesize = ftell(fp);
    fclose(fp);

    // 计算totalblocknum数量
    int totalblock = filesize / BLOCK_SIZE;
    if(filesize % BLOCK_SIZE > 0)
    {
        totalblock++;
    }
    unsigned char md5[16] = {0};
    MD5((const unsigned char*)path.c_str(), path.size(), md5);

    // 发起一个上传文件的请求到服务器
    UploadFile uploadfile;
    strcpy(uploadfile.filepath, path.c_str());
    uploadfile.filesize = filesize;
    // uploadfile.md5 = md5;
    memcpy(uploadfile.md5, md5, 16);
    ret = sendPacket(sockfd, TYPE_UPLOAD, (void*)&uploadfile, sizeof(UploadFile));
    CheckErr(ret, "sendPacket err");

    // 解析服务端传回的ack类型
    char buf[1024] = {0};
    ret = recvPacket(sockfd, buf, 1024);
    CheckErr(ret, "RecvPacket err");
    
    DataPacket *dp = (DataPacket*)buf;
    UploadFileAck uploadfileack;
    memcpy(&uploadfileack, dp->data, dp->datalen);

    //  判断进行传输的类型
    switch (uploadfileack.type)
    {
    case TYPE_QUICK:
        // 快传
        printf("quick upload was already completed!");
        break;
    case TYPE_NORMAL:
        // 普传
        int num = uploadfileack.blockno;
        
        int filefd = open(path.c_str(), O_RDONLY);
        CheckErr(filefd, "open err");
        lseek(filefd, (num-1)*BLOCK_SIZE, SEEK_SET);    // TODO

        while (num < totalblock)
        {
            // 组装TransferFile发送块到服务端
            TransferFile tfile;
            tfile.blockno = num;

            // char buf[BLOCK_SIZE] = {0};
            // int readsize = read(filefd, buf, BLOCK_SIZE);
            // memcmp(tfile.content, );

            int readsize = read(filefd, tfile.content, BLOCK_SIZE);
            int size = readsize < BLOCK_SIZE?sizeof(TransferFile):sizeof(int)+readsize;// 读到最后一块精确计算
            ret = sendPacket(sockfd, TYPE_UPLOAD, (void*)&tfile, size);
            CheckErr(ret, "SendPacket err");

            // 接受服务端反馈，显示进度
            char buf[1024] = {0};
            ret = recvPacket(sockfd, buf, 1024);
            CheckErr(ret, "recvPacket err");
            DataPacket *dp = (DataPacket*)buf;
            if(dp->type == TYPE_ACK)
            {
                // 显示进度
                if(strlen(dp->data) > 0)
                {
                    printf("...%s ", dp->data);
                }
            }
            num++;
        }
        printf("\n");
        close(filefd);

        // 文件传输结束，检验md5是否正确
        char buf[1024] = {0};
        ret = recvPacket(sockfd, buf, 1024);
        CheckErr(ret, "RecvPacket err");
        DataPacket *dp = (DataPacket*)buf;
        if(strncmp(dp->data, "ok", 2))
        {
            printf("file transfer was completed!");
        }
        else
        {
            printf("file transfer was occurred error!");
        }
        break;
    default:
        break;
    }

}