#include <stdio.h>
#include <curl/curl.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <sys/stat.h>

#define THREAD_NUM  10
struct fileInfo ** pinfoTable;

struct fileInfo
{
    char *fileptr;      // mmap开辟的内存指针
    int offset;         // mmap开辟的内存偏移量
    int end;            // 每块数据块的尾部偏移量
    pthread_t threadid; // 当前threadid
    const char *url;    // 请求url
    int downloadLength; // 下载的数据长度
    int remainDownload; // 剩余下载长度
};

// double 无符号整数防止溢出
double getDownloadFileLength(const char *url)
{
    // 发送1字节数据请求url，可能会失败，如果对方设置了curl限制
    double downloadFileLength = -1;
    CURL *curl = curl_easy_init();
   
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HEADER, 1);
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1);

    CURLcode code = curl_easy_perform(curl);
    if(CURLE_OK == code)
    {
        curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &downloadFileLength);
    }
    curl_easy_cleanup(curl);
    return downloadFileLength;
}

// curl下载进度通知
int progress_callback(void *userdata, double dltotal, double dlnow, double ultotal, double ulnow)
{
    // 记录info下载的长度
    struct fileInfo *info = (struct fileInfo *)userdata;
    info->downloadLength = dlnow;
	info->remainDownload = dltotal;

    double alreadyDownloadLength = 0;
    double remainTotalLength = 0;
    if (dltotal > 0) 
    {
        for (int i = 0;i <= THREAD_NUM;i ++) 
        {
            alreadyDownloadLength += pinfoTable[i]->downloadLength;
			remainTotalLength += pinfoTable[i]->remainDownload;
        }
    }
    static int print = 1;   // 静态局部对象，对访问此方法的所有对象保持一致
    int progress = alreadyDownloadLength / remainTotalLength * 100;
    if(print == progress)
    {
        printf("current progress of download is: %d%%\n", print++);
    }
    return 0;
}


// 服务端每次发回size大小的nmemb个的数据时就会触发,单位byte
size_t write_callback(void *ptr, size_t size, size_t nmemb, void *usrdata)
{
    // 从ptr中取出size * nmemb拷贝到info->fileptr + info->offset中
    struct fileInfo *info = (struct fileInfo *)usrdata;
    memcpy(info->fileptr + info->offset, ptr, size * nmemb);    // 数据存档
    info->offset += size * nmemb;
    return size * nmemb;
}

void* worker(void *arg)
{
    // 当前线程负责的range通过fileptr+offset实现首地址续写
    struct fileInfo *info = (struct fileInfo *)arg;

    // TODO 断点传输

    // 构造http请求头的range， 实现分片传输
    char range[64] = {0};
    snprintf(range, 64, "%d-%d", info->offset, info->end);
    printf("worker: threadid: %ld, download from: %d to: %d\n", info->threadid, info->offset, info->end);

    CURL *curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, info->url);

    // 数据写回调
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, info);        
    // 数据传输进度回调
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0);      // 启用传输进度功能
    curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, progress_callback);
    curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, info);

    curl_easy_setopt(curl, CURLOPT_RANGE, range);       // 设置切片范围
    CURLcode code = curl_easy_perform(curl);            // 阻塞执行,数据接受完才会返回
    if(CURLE_OK != code)
    {
        printf("code error: %d\n", code);
    }
    curl_easy_cleanup(curl);
    return NULL;
}

int download(const char *url, const char *filename)
{
    // 在本地先创建下载文件总大小的文件
    long fileLength = getDownloadFileLength(url);
    printf("downloadFileLength: %ld\n", fileLength);

    // 为fd授读写权限,如果不授权，sudo提供的是一次性权限，后续在内存空间的偏移操作将会出错
    int fd = open(filename, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if(-1 == fd)
    {
        return -1;
    }
    // 使fd偏移fileLength-1
    if(-1 == lseek(fd, fileLength-1, SEEK_SET))
    {
        perror("lseek fd\n");
        close(fd);
        return -1;
    }
    if(1 != write(fd, "", 1))  // 向fd中写入""保证fd存在
    {
        perror("write fd\n");
        close(fd);
        return -1;
    }
    // 使用mmap映射开辟内存空间
    char *fileptr = (char*)mmap(NULL, fileLength, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(MAP_FAILED == fileptr)
    {
        perror("mmap fd\n");
        close(fd);
        return -1;
    }

    // 开启多个线程分片下载文件 0-99 100-199 ...
    // 这里要创建THREAD_NUM+1个线程，最后一个线程负责剩余的数据下载
    int partSize = fileLength / THREAD_NUM;
    struct fileInfo *info[THREAD_NUM+1] = {NULL};
    for (size_t i = 0; i <= THREAD_NUM; i++)
    {
        info[i] = (struct fileInfo *)malloc(sizeof(struct fileInfo));   // 这里不能分配指针，需要分配对象实际大小
        memset(info[i], 0, sizeof(struct fileInfo));
        info[i]->fileptr = fileptr;
        info[i]->url = url;
        info[i]->downloadLength = 0;
        info[i]->offset = i * partSize;             // 每一片的起始位置
        // 在最后一段修正片的结束位置
        if(i < THREAD_NUM)
            info[i]->end = (i + 1) * partSize -1;   // 每一片的结束位置
        else
            info[i]->end = fileLength-1;
        pthread_create(&info[i]->threadid, NULL, worker, info[i]);
        usleep(0.3);    // 保证线程顺序执行
    }
    pinfoTable = info;
    
    // 回收资源
    for (size_t i = 0; i <= THREAD_NUM; i++)
    {
        pthread_join(info[i]->threadid, NULL);
    }
    for (size_t i = 0; i <= THREAD_NUM; i++)
    {
        free(info[i]);
    }
    munmap(fileptr, fileLength);
    close(fd);
    printf("download success\n");
    return 0;
}

// int main(int argc, char const *argv[])
// {
//     // 8.1M---md5sum检测
//     // c0a3cc386a7db943c8a09236ea4501d8  ubuntu.iso.zsync
//     // c0a3cc386a7db943c8a09236ea4501d8  orgin-ubuntu.iso.zsync
//     const char *url = "https://releases.ubuntu.com/focal/ubuntu-20.04.6-desktop-amd64.iso.zsync";
//     const char *filename = "ubuntu.iso.zsync";
//     return download(url, filename);
// }