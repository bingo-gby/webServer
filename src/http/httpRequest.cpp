#include "httpRequest.h"

const std::unordered_set<std::string> HttpRequest::DEFAULT_HTML{
    "/index", "/register", "/login",
    "/welcome", "/video", "/picture",
}; // 默认html文件

const std::unordered_map<std::string, int> HttpRequest::DEFAULT_HTML_TAG{
    {"/register.html", 0}, {"/login.html", 1}
}; // 默认html文件标签

void HttpRequest::init() {
    m_state = REQUEST_LINE;
    m_method = m_path = m_version = m_body = "";
    m_headers.clear();  // 请求头是key-value形式的，所以用unordered_map
    m_post.clear(); // POST请求的参数也是key-value形式的，所以用unordered_map
}

// http 1.1 支持持久连接，所以需要判断是否是keep-alive
bool HttpRequest::isKeepAlive() const {
    if (m_headers.count("Connection") == 1) {
        return m_headers.find("Connection")->second == "keep-alive" && m_version == "1.1";
    }
    return false;
}

// 解析请求行
bool HttpRequest::parse(Buffer& buff) {
    const char CRLF[] = "\r\n";  // 回车换行结束符
    if (buff.readableBytes() <= 0) {
        return false;
    }
    while (buff.readableBytes() && m_state != FINISH) {
        // 在buff中查找CRLF,后两个是查找字符 前面是查找的范围
        // 输出的是子字符串在主字符串中的位置
        const char* lineEnd = std::search(buff.peek(), buff.beginWrite(), CRLF, CRLF + 2); 
        std::string line(buff.peek(), lineEnd); // 转换数据，左闭右开区间
        switch (m_state) {
            case REQUEST_LINE:
                if (!parseRequestLine(line)) {
                    return false;
                }
                parsePath();
                break;
            case HEADERS:
                parseHeader(line);
                // 如果buff中只有一个回车换行，说明请求头解析完了
                if (buff.readableBytes() <= 2) {
                    m_state = FINISH;
                }
                break;
            case BODY:
                parseBody(line);
                break;
            default:
                break;
        }
        // 读完了
        if (lineEnd == buff.beginWrite()) {
            break;
        }
        buff.retrieveUntil(lineEnd + 2); // 跳过回车换行
    }
    LOG_DEBUG("request line: %s, %s, %s", m_method.c_str(), m_path.c_str(), m_version.c_str());
    return true;
}

// 解析路径，看是哪个html文件
void HttpRequest::parsePath() {
    if (m_path == "/") {
        m_path = "/index.html";
    } else {
        for (auto& item : DEFAULT_HTML) {
            if (item == m_path) {
                m_path += ".html";
                break;
            }
        }
    }
}

// 解析请求行
bool HttpRequest::parseRequestLine(const std::string& line) {
    // 在匹配规则中，以括号()的方式来划分组别 一共三个括号 [0]表示整体
    std::regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
    std::smatch subMatch;
    if (std::regex_match(line, subMatch, patten)) {
        m_method = subMatch[1];
        m_path = subMatch[2];
        m_version = subMatch[3];
        m_state = HEADERS;
        return true;
    }
    LOG_ERROR("Parse RequestLine Error");
    return false;
}

// 解析请求头
void HttpRequest::parseHeader(const std::string& line) {
    // key: value ？代表0次或1次
    std::regex patten("^([^:]*): ?(.*)$");
    std::smatch subMatch;
    if (std::regex_match(line, subMatch, patten)) {
        m_headers[subMatch[1]] = subMatch[2];  // key-value形式
    } else {
        m_state = BODY;  // 代表头部解析完了，要去解析请求体
    }
}

// 解析请求体
void HttpRequest::parseBody(const std::string& line) {
    m_body = line;
    parsePost();  // get请求没有请求体，所以只有post请求才需要解析请求体
    m_state = FINISH;
    LOG_DEBUG("Body: %s, len: %d", line.c_str(), line.size());
}

// 16进制转为10进制
int HttpRequest::ConverHex(char ch) {
    if ('0' <= ch && ch <= '9') {
        return ch - '0';
    } else if ('a' <= ch && ch <= 'f') {
        return ch - 'a' + 10;
    } else if ('A' <= ch && ch <= 'F') {
        return ch - 'A' + 10;
    } else {
        return -1;
    }
}
// 解析POST请求
