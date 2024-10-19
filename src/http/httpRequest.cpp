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
        const char* lineEnd = std::search(buff.peek(), const_cast<const char*>(buff.beginWrite()), CRLF, CRLF + 2); 
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
void HttpRequest::parsePost() {
    if (m_method == "POST" && m_headers["Content-Type"] == "application/x-www-form-urlencoded") {
        parseFromUrlencoded();
        if (DEFAULT_HTML_TAG.count(m_path)) {
            int tag = DEFAULT_HTML_TAG.find(m_path)->second;
            LOG_DEBUG("Tag: %d", tag);
            if (tag == 0 || tag == 1) {
                // 用户验证
                bool isLogin = (tag == 1);
                if (UserVerify(m_post["username"], m_post["password"], isLogin)) {
                    m_path = "/welcome.html";
                } else {
                    m_path = "/error.html";
                }
            }
        }
    }
}

// 解析url编码
// 是 key=value&key=value的形式
// 其中+ 和 %20 都是代表空格
// % 表示后面的两个字符是16进制的数，可以转为10进制
void HttpRequest::parseFromUrlencoded() {
    if (m_body.size() == 0) {
        return;
    }
    std::string key, value;
    int num = 0;
    int n = m_body.size();
    int i = 0, j = 0;
    // i是遍历m_body的下标
    for (; i < n; ++i) {
        char ch = m_body[i];
        switch (ch) {
            case '=':
                key = m_body.substr(j, i - j);
                j = i + 1;
                break;
            case '+':
                m_body[i] = ' ';
                break;
            case '%':
                num = HttpRequest::ConverHex(m_body[i + 1]) * 16 + HttpRequest::ConverHex(m_body[i + 2]);
                m_body[i + 2] = num % 10 + '0';
                m_body[i + 1] = num / 10 + '0';
                i += 2;
                break;
            case '&':
                value = m_body.substr(j, i - j);
                j = i + 1;
                m_post[key] = value;
                LOG_DEBUG("%s = %s", key.c_str(), value.c_str());
                break;
            default:
                break;
        }
    }
    // 遍历完之后，理论上是最后一个key没有value，此时 j是=后面，i是n
    assert(j <= i);
    if (m_post.count(key) == 0 && j < i) {
        value = m_body.substr(j, i - j);
        m_post[key] = value;
    }
}

// 类外定义静态函数不需要再写 static
// isLogin 代表是登录还是注册
bool HttpRequest::UserVerify(const std::string& name, const std::string& pwd, bool isLogin) {
    if (name == "" || pwd == "") {
        return false;
    }
    LOG_INFO("Verify name: %s pwd: %s", name.c_str(), pwd.c_str());
    MYSQL* sql;
    SqlConnRALL con(&sql, SqlConnPool::instance());  // 第一个参数是二重指针
    assert(sql);
    bool flag = false;
    unsigned int j = 0;
    char order[256] = {0};  // 用于构建sql语句
    MYSQL_RES* res = nullptr;  // 指向查询结果集的指针
    MYSQL_FIELD* field = nullptr; // 指向查询结果字段的指针
    if (!isLogin) { flag = true; } 
    // 从user表中查询用户名为name的记录，返回用户名和密码
    snprintf(order, 256, "SELECT username, password FROM user WHERE username='%s' LIMIT 1", name.c_str());
    LOG_DEBUG("%s", order);

    // 函数执行失败，返回1
    if(mysql_query(sql, order)) {
        mysql_free_result(res);
        return false;
    }
    // sql包含了查询结果
    res = mysql_store_result(sql);
    j = mysql_num_fields(res);  // 获取结果集中字段的数量
    field = mysql_fetch_fields(res);  // 获取结果集的字段信息，并存储在指针 fields 中

    // 一行行从结果集取数据，放到 row里
    while (MYSQL_ROW row = mysql_fetch_row(res)) {
        LOG_DEBUG("MYSQL ROW: %s %s", row[0], row[1]);  // row[0]是用户名，row[1]是密码
        std::string password(row[1]);
        // 看是不是登录操作
        if (isLogin) {
            if (pwd == password) {
                flag = true;
            } else {
                flag = false;
                LOG_DEBUG("pwd error!");
            }
        } else {
            flag = false;   
            LOG_DEBUG("user used!");   // 注册操作，查询到了该用户名，说明已经被注册了
        }
    }
    mysql_free_result(res);

    // 如果是注册操作，且用户名没有被注册过
    if (!isLogin && flag == true) {
        bzero(order, 256);  //  清空数组
        // 构建插入语句
        snprintf(order, 256, "INSERT INTO user(username, password) VALUES('%s', '%s')", name.c_str(), pwd.c_str());
        LOG_DEBUG("%s", order);
        if (mysql_query(sql, order)) {
            LOG_DEBUG("Insert error!");
            flag = false;
        } else {
            flag = true;   // 注册成功
        }
    }
    LOG_DEBUG("UserVerify success!");
    return flag;
}

std::string HttpRequest::path() const {
    return m_path;
}

std::string& HttpRequest::path() {
    return m_path;
}

std::string HttpRequest::method() const {
    return m_method;
}

std::string HttpRequest::version() const {
    return m_version;
}

std::string HttpRequest::getPost(const std::string& key) const {
    assert(key != "");
    if (m_post.count(key) == 1) {
        return m_post.find(key)->second;
    }
    return "";
}

// 字符数组，会隐式转换为string
std::string HttpRequest::getPost(const char* key) const {
    assert(key != nullptr);
    if (m_post.count(key) == 1) {
        return m_post.find(key)->second;
    }
    return "";
}

