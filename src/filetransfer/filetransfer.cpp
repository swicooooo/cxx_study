#include "filetransfer/filetransfer.h"

#include <string>
#include <vector>
#include <unistd.h>
#include <stdint.h>
#include <sys/socket.h>
#include <openssl/md5.h>

int filetransfer::sendPacket(int sockfd, OperType t, void *data, int datalen)
{
    // 构建数据包发送给客户端，分配新的DataPacket空间
    struct DataPacket *dp = (struct DataPacket*)new char[sizeof(struct DataPacket)+datalen];
    if(dp == NULL)
    {
        printf("sendPacket: malloc error!\n");
    }
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

int filetransfer::recvPacket(int sockfd, void *buf, int size)
{
    bzero(buf, size);
    return recv(sockfd, buf, size, 0);
}

filetransferServer::filetransferServer()
{
    mysql_ = std::make_unique<MySQL>(MySQL("file"));
}

/**
 * 1. path+name 不存在  普传
 * 2. path+name 存在    
 *          state == '1' && md5 == md5  秒传
 *          state == '1' && md5 != md5  名字相同的普传
 *          state == '0'                断点续传
 * */
void filetransferServer::processUpload(int clientfd, struct DataPacket *dp)

{
    clientfd_ = clientfd;   //TODO
    // 从data中解析为客户端传来的UploadFile
    struct UploadFile uploadFile;
    memcpy(&uploadFile, dp->data, dp->datalen);

    // 判定文件路径是否合法
    filePath_ = uploadFile.filepath;
    int idx = filePath_.find_last_of("/");;
    if(idx == std::string::npos)
    {
        std::string response = "illegal file path!" + filePath_;
        ret_ = sendPacket(clientfd, TYPE_ACK, (void*)response.c_str(), response.length());
        CheckErr(ret_, "sendPacket err!");
        return;
    }
    printf("filePath: %s\n", filePath_.c_str());

    // 记录上传文件信息
    path_ = filePath_.substr(0, idx+1);
    name_ = filePath_.substr(idx+1);
    filesize_ = uploadFile.filesize;
    md5_ = uploadFile.md5;

    // 查询数据库关于文件的信息
    char sql[1024] = {0};
    snprintf(sql, 1024, "select state,md5,blockno from file where name='%s' and path='%s'", name_.c_str(), path_.c_str());
    auto vec = this->queryFileInfo(sql);

    // 通知客户端接下来发送的块号
    this->instructClientSendBlock(clientfd, vec, uploadFile);
    
    // 通知完客户端，开始接收文件
    uint64_t recvsize = 0;
    this->recvClientSendBlock(recvsize);

    // 向数据库中写传输信息 blocknum
    this->updateFileState(recvsize, vec);
}

std::vector<std::string> filetransferServer::queryFileInfo(const char *sql)
{
    std::vector<std::string> vec{};
    MYSQL_RES *res = mysql_->query(sql);
    if(res != nullptr) {
        MYSQL_ROW row = mysql_fetch_row(res);
        if(row != nullptr)  {
            vec.push_back(row[0]);  // state
            vec.push_back(row[1]);  // md5
            vec.push_back(row[2]);  // blockno
            printf("queryFileInfo: state: %s,md5: %s,blockno: %s\n", row[0], row[1], row[2]);
            mysql_free_result(res);
        }
    }
    return vec;
}

void filetransferServer::instructClientSendBlock(int clientfd, const std::vector<std::string> &vec, const struct UploadFile &uploadFile)
{
    if(vec.empty())
    {
        printf("transfer: normal\n");
        // 普传 blockno：0
        struct UploadFileAck ack;
        ack.type = TYPE_NORMAL;
        ack.blockno = 0;
        ret_ = sendPacket(clientfd, TYPE_ACK, (void*)&ack, sizeof(ack));
        CheckErr(ret_, "sendPacket err!");
    }
    else 
    {
        printf("state: %s, md5: %s", vec[0].c_str(), vec[1].c_str());
        if(vec[0] == "1" && vec[1] == uploadFile.md5)
        {
            // 秒传
            printf("transfer: quick\n");
            struct UploadFileAck ack;
            ack.type = TYPE_QUICK;
            ret_ = sendPacket(clientfd, TYPE_ACK, (void*)&ack, sizeof(ack));
            CheckErr(ret_, "sendPacket err!");
        }
        else if(vec[0] == "1" && vec[1] != uploadFile.md5)
        {
            // 普传，名称一致但是不同文件
            printf("transfer: normal will be modified name\n");
            struct UploadFileAck ack;
            ack.type = TYPE_NORMAL;
            ack.blockno = 0;   
            ret_ = sendPacket(clientfd, TYPE_ACK, (void*)&ack, sizeof(ack));
            CheckErr(ret_, "sendPacket err!");
            name_ += "-md5";     // 重写文件名
        }
        else
        {
            // 断点续传, 不完整数据
            printf("transfer: breakpoint continue\n");
            struct UploadFileAck ack;
            ack.type = TYPE_NORMAL;
            ack.blockno = atoi(vec[2].c_str())+1;
            ret_ = sendPacket(clientfd, TYPE_ACK, (void*)&ack, sizeof(ack));
            CheckErr(ret_, "sendPacket err!");
        }
    }
}

void filetransferServer::recvClientSendBlock(uint64_t &recvsize)
{
    // 根据文件是否有内容改变打开方式
    int filefd = access(filePath_.c_str(), F_OK)==0?
                open(filePath_.c_str(), O_APPEND | O_RDWR):
                open(filePath_.c_str(), O_CREAT | O_RDWR | O_APPEND);
	CheckErr(filefd, "open err");

    // 开始循环接收DataPacket(附带TransferFile长度)
    const int size = sizeof(DataPacket) + sizeof(TransferFile);
    char recvbuf[size] = {0};
    while (1)
    {
        ret_ = recvPacket(clientfd_, recvbuf, size);
        CheckErr(ret_, "recvPacket err");
        if(ret_ == 0)
        {
            break;  // 客户端断开
        }

        // 将客户端发来的包进行转换
        DataPacket *dp = (DataPacket*)recvbuf;
        TransferFile *tfile = (TransferFile*)dp->data;
        int writesize = dp->datalen - sizeof(int);  // datalen存的是TransferFile的长度,需减去int的长度
        ret_ = write(filefd, dp->data, writesize);
        CheckErr(ret_, "write err");

        // 记录, 可选返回ack进度
        blocknum_++;
        recvsize += writesize;

        printf("recvsize: %ld, filesize: %ld", recvsize, filesize_);
        if(recvsize == filesize_)
        {
            // 接收完成
            break;
        }
    }
    close(filefd);
}

void filetransferServer::updateFileState(uint64_t &recvsize, const std::vector<std::string> &vec)
{
    char sql[1024] = {0};
    // 完全接收，校验完md5后向客户端发送ack提示
    if(recvsize == filesize_)
    {
        // 使用openssl的MD5加密与客户端发来的MD5对比
        unsigned char gmd5[16] = {0};
        MD5((const unsigned char*)filePath_.c_str(), filePath_.length(), gmd5);

        // MD5相同，响应ack文件传输成功
        if((memcmp(gmd5, md5_.c_str(), 16)) == 0)
        {
            // 如果数据库存在数据则更新数据，否则插入新数据
            if(vec.empty())
            {
                snprintf(sql, 1024, "insert into file(name, path, md5, state, blockno) values('%s', '%s', '%s', '%s', %d)",
					name_.c_str(), path_.c_str(), md5_.c_str(), "1", blocknum_);
            }
            else
            {
                snprintf(sql, 1024, "update file set state='1',blockno=%d where name='%s' and path='%s'",
					blocknum_, name_.c_str(), path_.c_str());
            }
            mysql_->update(sql);
            ret_ = sendPacket(clientfd_, TYPE_ACK, (void*)"ok", strlen("ok"));
            CheckErr(ret_, "SendPacket err");
            printf("updateFileState: success to update or insert info to db!\n");
        }
        else
        {
            // MD5不相同，响应ack文件传输失败，删除本地文件
            ret_ = sendPacket(clientfd_, TYPE_ACK, (void*)"fail", strlen("fail"));
            CheckErr(ret_, "SendPacket err");
            printf("updateFileState: fail to update or insert info to db!\n");
        }
    }
    else
    {
        // 不完全接收，因为clientfd中断提前跳出
		if (vec.empty())
		{
			snprintf(sql, 1024, "insert into file values(name, path, md5, state, blockno) values('%s', '%s', '%s', '%s', %d)",
					name_.c_str(), path_.c_str(), md5_.c_str(), "0", blocknum_);
		}
        else
		{   
            // 如果存在更新blockno(ps:state="0")
			snprintf(sql, 1024, "update file set blockno=%d where name='%s' and path='%s'"
				,blocknum_, name_.c_str(), path_.c_str());
		}
        mysql_->update(sql);
        printf("updateFileState: transfer incomletedly!\n");
    }
}

void filetransferClient::uploadFile(int sockfd, const std::string &path)
{
     printf("uploadFile start...!\n");
    sockfd_ = sockfd;
    // 获取文件大小
    auto filesize = this->getFileSize(path);

    // 计算totalblocknum数量
    int totalblock = filesize / BLOCK_SIZE;
    if(filesize % BLOCK_SIZE > 0)
    {
        totalblock++;
    }
    // 生成MD5
    unsigned char md5[16] = {0};
    MD5((const unsigned char*)path.c_str(), path.size(), md5);

    // 发送上传文件的请求
    struct UploadFile uploadfile;
    strcpy(uploadfile.filepath, path.c_str());
    uploadfile.filesize = filesize;
    memcpy(uploadfile.md5, md5, 16);
    ret_ = sendPacket(sockfd, TYPE_UPLOAD, (void*)&uploadfile, sizeof(struct UploadFile));
    CheckErr(ret_, "sendPacket err");

    // 解析服务端传回的ack类型
    char buf[1024] = {0};
    ret_ = recvPacket(sockfd, buf, 1024);
    CheckErr(ret_, "RecvPacket err");

    // 从DataPacket中取出服务端传递给客户端的数据
    struct DataPacket *dp = (DataPacket*)buf;
    struct UploadFileAck uploadfileack;
    memcpy(&uploadfileack, dp->data, dp->datalen);

    // 通过服务端返回的传输类型发送数据块
    this->sendBlockByType(uploadfileack, totalblock, path);
}

uint64_t filetransferClient::getFileSize(const std::string &path)
{
    // 直接偏移fd的位置来计算文件大小
    FILE *fp = fopen(path.c_str(), "r");
    CheckErr(fp, "fopen err");
    fseek(fp, 0, SEEK_END);
    uint64_t filesize = ftell(fp);
    fclose(fp);
    return filesize;
}

void filetransferClient::sendBlockByType(const struct UploadFileAck &uploadfileack, int totalblock, const std::string &path)
{
    std::string res;
    //  判断进行传输的类型
    switch (uploadfileack.type)
    {
    case TYPE_QUICK:
        // 快传
        printf("quick upload was already completed!");
        break;
    case TYPE_NORMAL:
        // 普传，根据服务端返回的blocknum移动filefd的读指针
        int num = uploadfileack.blockno;
        int filefd = open(path.c_str(), O_RDONLY);
        CheckErr(filefd, "open err");
        lseek(filefd, num*BLOCK_SIZE, SEEK_SET);  

        // 发送数据块
        while (num < totalblock)
        {
            // 组装TransferFile发送块到服务端
            TransferFile tfile;
            tfile.blockno = num++;
            int readsize = read(filefd, tfile.content, BLOCK_SIZE);
            int size = readsize < BLOCK_SIZE?sizeof(int)+readsize:sizeof(TransferFile);// 读到最后一块精确计算
            ret_ = sendPacket(sockfd_, TYPE_UPLOAD, (void*)&tfile, size);
            CheckErr(ret_, "SendPacket err");

            // TODO 接受服务端反馈，显示进度
        }
        close(filefd);

        // 文件传输结束，检验md5是否正确
        char buf[1024] = {0};
        ret_ = recvPacket(sockfd_, buf, 1024);
        CheckErr(ret_, "RecvPacket err");
        DataPacket *dp = (DataPacket*)buf;

        // 打印文件传输结果
        res = strncmp(dp->data, "ok", 2)?
                            "file transfer was completed!":
                            "file transfer was occurred error!";
        printf("file message: %s", res.c_str());
        break;
    }
}
