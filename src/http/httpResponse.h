#ifndef HTTPRESPONSE_H
#define HTTPRESPONSE_H

#include <unordered_map>
#include <fcntl.h> //用于文件描述符的操作
#include <unistd.h> //用于对文件描述符 API 的操作
#include <sys/stat.h> //用于文件状态的操作
#include <sys/mman.h> //用于内存映射

#include "../buffer/buffer.h"
#include "../log/log.h"

class HttpResponse {

public:
    HttpResponse();
    ~HttpResponse();

    void init(const std::string& srcDir, std::string& path, bool isKeepAlive = false, int code = -1);
    //生成响应报文，写入到 buff 中
    void makeResponse(Buffer& buff);  
    //释放内存映射
    void unmapFile();  
    // 返回映射文件的指针
    char* file();   
    // 返回映射文件的长度
    size_t fileLen() const;  
    // 生成错误响应报文，并写入buff中
    void errorContent(Buffer& buff, std::string message);  
    // 返回状态码
    int code() const { return m_code; }  
private:
    //添加状态行
    void addStateLine_(Buffer& buff); 
    //添加响应头
    void addHeader_(Buffer& buff);   
    //添加响应体
    void addContent_(Buffer& buff);  
    // 生成错误响应报文的 HTML 内容
    void errorHtml_();   
    // 获取文件类型
    std::string getFileType_();  

    // 状态码
    int m_code;  
    // 是否保持连接
    bool m_isKeepAlive;  

    // 请求路径
    std::string m_path;     
    // 资源目录
    std::string m_srcDir;  

    // 映射文件指针
    char* m_mmFile;    
    // 映射文件状态
    struct stat m_mmFileStat;  

    // 文件后缀类型,根据响应的文件类型返回对应的 Content-Type
    static const std::unordered_map<std::string, std::string> SUFFIX_TYPE;  
    // 状态码对应的状态信息
    static const std::unordered_map<int, std::string> CODE_STATUS;   
    // 状态码对应的 .html 文件路径
    static const std::unordered_map<int, std::string> CODE_PATH;   
};

#endif // HTTPRESPONSE_H