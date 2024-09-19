#include "httpResponse.h"

const std::unordered_map<std::string, std::string> HttpResponse::SUFFIX_TYPE {
    {".html", "text/html"},
    {".xml", "text/xml"},
    {".xhtml", "application/xhtml+xml"},
    {".txt", "text/plain"},
    {".rtf", "application/rtf"},
    {".pdf", "application/pdf"},
    {".word", "application/msword"},
    {".png", "image/png"},
    {".gif", "image/gif"},
    {".jpg", "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".au", "audio/basic"},
    {".mpeg", "video/mpeg"},
    {".mpg", "video/mpeg"},
    {".avi", "video/x-msvideo"},
    {".gz", "application/x-gzip"},
    {".tar", "application/x-tar"},
    {".css", "text/css "},
    {".js", "text/javascript "},
};


const std::unordered_map<int, std::string> HttpResponse::CODE_STATUS {
    {200, "OK"},
    {400, "Bad Request"},    // 400 代表客户端请求的语法错误，服务器无法理解
    {403, "Forbidden"},
    {404, "Not Found"},
};

const std::unordered_map<int, std::string> HttpResponse::CODE_PATH {
    {400, "/400.html"},
    {403, "/403.html"},
    {404, "/404.html"},
};

HttpResponse::HttpResponse() {
    m_code = -1;
    m_isKeepAlive = false;
    m_srcDir = "";
    m_mmFile = nullptr;
    m_mmFileStat = {0};
}

HttpResponse::~HttpResponse() {
    unmapFile(); //释放内存映射
}

void HttpResponse::init(const std::string& srcDir, std::string& path, bool isKeepAlive, int code) {
    assert(!path.empty());
    if (m_mmFile) { unmapFile(); }  // 如果之前有映射文件，先释放
    m_code = code;
    m_isKeepAlive = isKeepAlive;
    m_path = path;
    m_srcDir = srcDir;
    m_mmFile = nullptr;
    m_mmFileStat = {0};
}

void HttpResponse::makeResponse(Buffer& buff) {
    // 判断请求的资源文件是否存在
    // stat() 用于获取文件的属性，存储在 m_mmFileStat 中 ，后面是是检查是否是目录
    if (stat((m_srcDir + m_path).data(), &m_mmFileStat) < 0 || S_ISDIR(m_mmFileStat.st_mode)) {
        m_code = 404;
    } else if (!(m_mmFileStat.st_mode & S_IROTH)) {
        m_code = 403;                     // 检查文件是否对他人可读 ，S_IROTH 是个宏，表示其他人可读的权限位，设置了即可读
    } else if (m_code == -1) {
        m_code = 200;
    }
    errorHtml_();  // 生成错误响应报文的 HTML 内容
    addStateLine_(buff);  //添加状态行
    addHeader_(buff);    //添加响应头
    addContent_(buff);   //添加响应体
}

char* HttpResponse::file() {
    return m_mmFile;
}

size_t HttpResponse::fileLen() const {
    return m_mmFileStat.st_size;
}

void HttpResponse::errorHtml_() {
    if (CODE_PATH.count(m_code) == 1) {
        m_path = CODE_PATH.find(m_code)->second;
        stat((m_srcDir + m_path).data(), &m_mmFileStat); // 获取错误文件的属性
    }
}

void HttpResponse::addStateLine_(Buffer& buff) {
    std::string status;  // 只有 200 400 需要处理
    if (CODE_STATUS.count(m_code) == 1) {
        status = CODE_STATUS.find(m_code)->second;
    } else {
        m_code = 400;
        status = CODE_STATUS.find(400)->second;
    }
    buff.append("HTTP/1.1 " + std::to_string(m_code) + " " + status + "\r\n");
}

void HttpResponse::addHeader_(Buffer& buff) {
    buff.append("Connection: ");
    if (m_isKeepAlive) {
        buff.append("keep-alive\r\n");
        buff.append("keep-alive: max=6, timeout=120\r\n");
    } else {
        buff.append("close\r\n");
    }
    buff.append("Content-type: " + getFileType_() + "\r\n");
}

void HttpResponse::addContent_(Buffer& buff) {
    int srcFd = open((m_srcDir + m_path).data(), O_RDONLY);  // 只读方式打开文件
    // -1 表示打开失败
    if (srcFd < 0) {
        errorContent(buff, "File NotFound!");
        return;
    }
    ////将文件映射到内存提高文件的访问速度  MAP_PRIVATE 建立一个写入时拷贝的私有映射
    LOG_DEBUG("file path %s", (m_srcDir + m_path).data());
    int* mmRet = (int*)mmap(0, m_mmFileStat.st_size, PROT_READ, MAP_PRIVATE, srcFd, 0);
    if (*mmRet == -1) {
        errorContent(buff, "File NotFound!");
        return;
    }
    m_mmFile = (char*)mmRet;
    close(srcFd);
    buff.append("Content-length: " + std::to_string(m_mmFileStat.st_size) + "\r\n\r\n");
}

  