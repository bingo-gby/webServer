#ifndef HTTPREQUEST_H
#define HTTPREQUEST_H

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <regex>
#include <errno.h>
#include <mysql/mysql.h>

#include "../buffer/buffer.h"
#include "../log/log.h"
#include "../pool/sqlconnpool.h"

// 写如何处理请求报文的
class HttpRequest{
public:
    enum PARSE_STATE{
        REQUEST_LINE,
        HEADERS,
        BODY,
        FINISH,
    };
    HttpRequest() { init(); }
    ~HttpRequest() = default;

    void init();
    bool parse(Buffer& buff);

    std::string path() const;
    std::string& path();
    std::string method() const;
    std::string version() const;
    std::string getPost(const std::string& key) const; // 获取POST请求的参数
    std::string getPost(const char* key) const; // 获取POST请求的参数

    bool isKeepAlive() const;

private:
    bool parseRequestLine(const std::string& line); // 解析请求行
    void parseHeader(const std::string& line);   // 解析请求头
    void parseBody(const std::string& line);    // 解析请求体

    void parsePath();   // 解析路径
    void parsePost();   // 解析POST请求
    void parseFromUrlencoded(); // 解析url编码

    static bool UserVerify(const std::string& name, const std::string& pwd, bool isLogin);  // 用户验证
    static int ConverHex(char ch);  // 16进制转为10进制

    PARSE_STATE m_state;
    std::string m_method, m_path, m_version, m_body;
    std::unordered_map<std::string, std::string> m_headers;
    std::unordered_map<std::string, std::string> m_post;

    static const std::unordered_set<std::string> DEFAULT_HTML;   // 默认html文件
    static const std::unordered_map<std::string, int> DEFAULT_HTML_TAG; // 默认html文件标签
    static int ConverHex(char ch);
};

#endif // HTTPREQUEST_H