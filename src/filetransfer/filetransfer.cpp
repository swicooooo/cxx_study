#include "filetransfer/filetransfer.h"

#include <string>
#include <vector>
#include <unistd.h>
#include <stdint.h>
#include <sys/socket.h>
#include <openssl/md5.h>
#include <sstream>

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
    mysql_ = new MySQL("file");    // 在堆上创建指针
}

filetransferServer::~filetransferServer()
{
    free(mysql_);
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
    mysql_->connect();      // 连接到数据库
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

    printf("the filePath of client upload : %s\n", filePath_.c_str());

    // 记录上传文件信息
    path_ = filePath_.substr(0, idx+1);
    name_ = filePath_.substr(idx+1);
    filesize_ = uploadFile.filesize;
    memcpy(md5_, uploadFile.md5, 33);

    // 查询数据库关于文件的信息
    char sql[1024] = {0};
    snprintf(sql, 1024, "select state,md5,blockno from file where name='%s' and path='%s'"
        ,name_.c_str(), path_.c_str());
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
    std::vector<std::string> vec;
    MYSQL_RES *res = mysql_->query(sql);
    if(res != nullptr) {
        MYSQL_ROW row = mysql_fetch_row(res);
        if(row != nullptr)  {
            vec.push_back(row[0]);  // state
            vec.push_back(row[1]);  // md5
            vec.push_back(row[2]);  // blockno
            mysql_free_result(res);
        }
    }
    return vec;
}

void filetransferServer::instructClientSendBlock(int clientfd, const std::vector<std::string> &vec, const struct UploadFile &uploadFile)
{
    if(vec.empty())
    {
        // 普传 blockno：0
        struct UploadFileAck ack;
        ack.type = TYPE_NORMAL;
        ack.blockno = 0;
        printf("transfer: normal\n");
        ret_ = sendPacket(clientfd, TYPE_ACK, (void*)&ack, sizeof(ack));
        CheckErr(ret_, "sendPacket err!");
    }
    else 
    {
        printf("state: %s, md5: %s", vec[0].c_str(), vec[1].c_str());
        if(vec[0] == "1" && vec[1] == (char*)uploadFile.md5)
        {
            // 秒传
            struct UploadFileAck ack;
            ack.type = TYPE_QUICK;
            printf("transfer: quick\n");
            ret_ = sendPacket(clientfd, TYPE_ACK, (void*)&ack, sizeof(ack));
            CheckErr(ret_, "sendPacket err!");
        }
        else if(vec[0] == "1" && vec[1] != (char*)uploadFile.md5)
        {
            // 普传，名称一致但是不同文件
            struct UploadFileAck ack;
            ack.type = TYPE_NORMAL;
            ack.blockno = 0;   
            printf("transfer: normal will be modified name\n");
            ret_ = sendPacket(clientfd, TYPE_ACK, (void*)&ack, sizeof(ack));
            CheckErr(ret_, "sendPacket err!");
            name_ += "-md5";     // 重写文件名
        }
        else
        {
            // 断点续传, 不完整数据
            struct UploadFileAck ack;
            ack.type = TYPE_NORMAL;
            ack.blockno = atoi(vec[2].c_str())+1;
            printf("transfer: breakpoint continue\n");
            ret_ = sendPacket(clientfd, TYPE_ACK, (void*)&ack, sizeof(ack));
            CheckErr(ret_, "sendPacket err!");
        }
    }
}

void filetransferServer::recvClientSendBlock(uint64_t &recvsize)
{
    // 根据文件是否有内容改变打开方式
    int filefd = access(name_.c_str(), F_OK)==0?
                open(name_.c_str(), O_APPEND | O_RDWR):
                open(name_.c_str(), O_CREAT | O_RDWR);
	CheckErr(filefd, "open err");

    // 开始循环接收DataPacket(附带TransferFile长度)
    const int size = sizeof(struct DataPacket) + sizeof(TransferFile);
    char recvbuf[size] = {0};
    while (recvsize < filesize_)
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
        // datalen存的是TransferFile的长度,需减去int的长度
        // 并且存储在dp->data中，所以写的时候需要偏移4个字节
        uint64_t writesize = dp->datalen - sizeof(int);  
        ret_ = write(filefd, dp->data + sizeof(int), writesize);  
        CheckErr(ret_, "write err");

        // 记录, 可选返回ack进度
        recvsize += writesize;
        printf("write fd recvsize: %ld, filesize: %ld\n", recvsize, filesize_);
    }
    close(filefd);
}

void filetransferServer::updateFileState(uint64_t &recvsize, const std::vector<std::string> &vec)
{
    char sql[1024] = {0};
    struct TransferFileAck transferFileAck;
    // 在数据库中记录 block 的信息
    if (recvsize == filesize_)
    {
        // 重新生成该文件的md5值，与客户端携带的md5对比
        // md5相同，响应ack文件传输成功
        unsigned char gmd5[16];
        MD5((const unsigned char*)filePath_.c_str(), filePath_.length(), gmd5);
        if (memcmp(md5_, gmd5, 33) == 0)
        {
            // 数据库不存在数据，插入新的数据
            if (vec.empty())
            {
				snprintf(sql, 1024, "insert into file(name, path, md5, state, blockno) values('%s', '%s', '%s', '%s', %d)",
					name_.c_str(), path_.c_str(),md5_, "1", blocknum_);
            }
            else
            {
                // 数据库存在数据，更新文件的状态
                snprintf(sql, 1024, "update file set state='1',blockno=%d where name='%s' and path='%s'",
					blocknum_, name_.c_str(), path_.c_str());
            }
            // 响应客户端
            mysql_->update(sql);
            transferFileAck.flag = '1';
        }
        else 
        {
            // md5不相同，传输出错，删除文件
            transferFileAck.flag = '0';
        }
        ret_ = sendPacket(clientfd_, TYPE_UPLOAD, (void*)&transferFileAck, sizeof(transferFileAck));
        CheckErr(ret_, "SendPacket err");
    }
    else
    {
        // 数据传输中断，客户端断开连接,向数据库中记录已发送的blocknum
		if (vec.empty())
		{
			snprintf(sql, 1024, "insert into file values(name, path, md5, state, blockno) values('%s', '%s', '%s', '%s', %d)",
					name_.c_str(), path_.c_str(), md5_, "0", blocknum_);
		}
        else
		{
			snprintf(sql, 1024, "update file set blockno=%d where name='%s' and path='%s'"
				,blocknum_, name_.c_str(), path_.c_str());
		}
        mysql_->update(sql);
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
    unsigned char md5[33] = {0};
    MD5((const unsigned char*)path.c_str(), path.size(), md5);
    printf("filesize: %ld, totalblock: %d, md5: %s!\n", filesize, totalblock, (char*)md5);

    // 发送上传文件的请求
    struct UploadFile uploadfile;
    strcpy(uploadfile.filepath, path.c_str());
    uploadfile.filesize = filesize;
    memcpy(uploadfile.md5, md5, 33);
    ret_ = sendPacket(sockfd, TYPE_UPLOAD, (void*)&uploadfile, sizeof(struct UploadFile));
    CheckErr(ret_, "sendPacket err");

    printf("sendPacket request upload success!\n");

    // 解析服务端传回的ack类型
    char buf[1024] = {0};
    ret_ = recvPacket(sockfd, buf, 1024);
    CheckErr(ret_, "RecvPacket err");
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
            // 计算放到DataPacket里面数据的长度
            int size = readsize < BLOCK_SIZE?sizeof(int)+readsize:sizeof(TransferFile);
            ret_ = sendPacket(sockfd_, TYPE_UPLOAD, (void*)&tfile, size);
            CheckErr(ret_, "SendPacket err");

            // TODO 接受服务端反馈，显示进度
            printf("progress: %d/%d, blocknum: %d\n", num, totalblock, num);
        }
        close(filefd);

        // 文件传输结束，检验md5是否正确
        char buf[1024] = {0};
        ret_ = recvPacket(sockfd_, buf, 1024);
        CheckErr(ret_, "RecvPacket err");
        DataPacket *dp = (DataPacket*)buf;
        TransferFileAck transferFileAck;
        memcpy(&transferFileAck, dp->data, dp->datalen);

        // 打印文件传输结果
        res = transferFileAck.flag == '1'?
                            "file transfer was completed!":
                            "file transfer was occurred error!";
        printf("file message: %s\n", res.c_str());
        break;
    }
}

