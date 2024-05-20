#include <stdio.h>
#include <curl/curl.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

struct fileInfo
{
    char *fileptr;  // mmap开辟的内存指针
    int offset;     // mmap开辟的内存偏移量
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

// 服务端每次发回size大小的nmemb个的数据时就会触发,单位byte
size_t write_callback(void *ptr, size_t size, size_t nmemb, void *usrdata)
{
    printf("write_callback: %ld\n", size * nmemb);
    // 从ptr中取出size * nmemb拷贝到info->fileptr + info->offset中
    struct fileInfo *info = (struct fileInfo *)usrdata;
    memcpy(info->fileptr + info->offset, ptr, size * nmemb);    // 数据存档
    info->offset += size * nmemb;
    return size * nmemb;
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

    struct fileInfo *info = (struct fileInfo *)malloc(sizeof(struct fileInfo));
    info->fileptr = fileptr;
    info->offset = 0;

    // 调用curl向url发起请求并设定写时回调函数

    CURL *curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, info);    // 传入write_callback的参数
    CURLcode code = curl_easy_perform(curl);            // 阻塞执行,数据接受完才会返回
    if(CURLE_OK != code)
    {
        printf("code error: %d\n", code);
    }

    // 回收资源
    curl_easy_cleanup(curl);
    munmap(fileptr, fileLength);
    close(fd);
    free(info);
    printf("download success\n");
    return 0;
}

int main(int argc, char const *argv[])
{
    // 8.1M---md5sum检测
    // c0a3cc386a7db943c8a09236ea4501d8  ubuntu.iso.zsync
    // c0a3cc386a7db943c8a09236ea4501d8  orgin-ubuntu.iso.zsync
    const char *url = "https://releases.ubuntu.com/focal/ubuntu-20.04.6-desktop-amd64.iso.zsync";
    const char *filename = "ubuntu.iso.zsync";
    return download(url, filename);
}
