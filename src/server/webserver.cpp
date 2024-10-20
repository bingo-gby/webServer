#include "webserver.h"

webServer::webServer(int port, int trigMode, int timeoutMS, 
                     bool OptLinger, int sqlPort, const char* sqlUser, 
                     const char* sqlPwd, const char* dbName, 
                     int connPoolNum, int threadNum, bool openLog, 
                     int logLevel, int logQueSize) :
                     port_(port), openLinger_(OptLinger), timeoutMS_(timeoutMS), isClose_(false),
      timer_(new HeapTimer()), threadpool_(new ThreadPool(threadNum)), epoller_(new Epoller())
{
    srcDir_ = getcwd(nullptr, 256); // 获取当前工作目录
    assert(srcDir_);
    strncat(srcDir_, "/resources/", 16); // 拼接资源目录
    HttpConn::userCount = 0;  // 初始化用户数量
    HttpConn::srcDir = srcDir_; // 设置资源目录

    // 是否打开日志标志
    if(openLog) {
        Log::instance()->init(logLevel, "./log", ".log", logQueSize);
        if(isClose_) {
            LOG_ERROR("Server init error");
            exit(1);
        } else {
            LOG_INFO("Server init success");
        }
    }

    initEventMode_(trigMode);  // 初始化事件模式
    //  初始化数据库连接池
    SqlConnPool::instance()->init("localhost", sqlPort, sqlUser, sqlPwd, dbName, connPoolNum);
    if(initSocket_()) { // 初始化socket
        isClose_ = false;
        LOG_INFO("Init socket success");
    } else {
        LOG_ERROR("Init socket error");
        isClose_ = true;
    }
}

webServer::~webServer() {
    close(listenFd_);
    isClose_ = true;
    free(srcDir_);
    SqlConnPool::instance()->closePool();
}

void webServer::initEventMode_(int trigMode) {
    listenEvent_ = EPOLLRDHUP;  // 关闭检测socket连接的关闭
    // 连接事件时关注对端关闭连接的情况，并且该事件是一次性的，触发后需要重新设置。
    connEvent_ = EPOLLONESHOT | EPOLLRDHUP; 
    switch (trigMode) {
        case 0:
            break;
        case 1:
            connEvent_ |= EPOLLET;
            break;
        case 2:
            listenEvent_ |= EPOLLET;
            break;
        case 3:
            listenEvent_ |= EPOLLET;
            connEvent_ |= EPOLLET;
            break;
        default:
            listenEvent_ = EPOLLRDHUP;
            connEvent_ = EPOLLONESHOT | EPOLLRDHUP;
            break;
    }
    // 表示HttpConn 是否开启ET模式
    HttpConn::isET = (connEvent_ & EPOLLET); 
}

// 
void webServer::start(){
    int timeMS = -1; // epoll_wait() 的超时时间为-1，表示永久阻塞
    if(!isClose_) {
        LOG_INFO("Server start");
    }
    while(!isClose_) {
        if(timeoutMS_ > 0) {
            timeMS = timer_->getNextTick(); // 获取定时器的超时时间
        }
        int eventCnt = epoller_->wait(timeMS); // 等待事件数目
        for(int i = 0; i < eventCnt; i++) {
            int fd = epoller_->getEventFd(i); // 获取事件的文件描述符
            uint32_t events = epoller_->getEvents(i); // 获取事件
            if(fd == listenFd_) {
                dealListen_(); // 处理监听事件
            } else if(events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                assert(users_.count(fd) > 0);
                closeConn_(&users_[fd]); // 关闭连接
            } else if(events & EPOLLIN) {
                if(users_.count(fd) > 0) {
                    onRead_(&users_[fd]); // 读事件
                } else {
                    LOG_ERROR("Error: fd no exist");
                }
            } else if(events & EPOLLOUT) {
                if(users_.count(fd) > 0) {
                    onWrite_(&users_[fd]); // 写事件
                } else {
                    LOG_ERROR("Error: fd no exist");
                }
            } else {
                LOG_ERROR("Error: something else");
            }
        }
    }
}

// 发送错误信息到客户端，info为错误信息
void webServer::sendError_(int fd, const char* info) {
    assert(fd > 0);
    int ret = send(fd, info, strlen(info), 0);
    if(ret < 0) {
        LOG_WARN("send error to client[%d] error!", fd);
    }
    close(fd);
}

void webServer::closeConn_(HttpConn* client) {
    assert(client);
    LOG_INFO("Client[%d] quit!", client->getFd());
    epoller_->delFd(client->getFd());
    client->httpclose();
}

void webServer::addClient_(int fd, sockaddr_in addr) {
    assert(fd > 0);
    users_[fd].init(fd, addr);
    if(timeoutMS_ > 0) {
        timer_->add(fd, timeoutMS_, std::bind(&webServer::closeConn_, this, &users_[fd]));
    }
    epoller_->addFd(fd, EPOLLIN | connEvent_);
    setFdNonblock(fd);
    LOG_INFO("Client[%d] in!", users_[fd].getFd());
}

void webServer::dealListen_() {
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    do {
        int fd = accept(listenFd_, (struct sockaddr*)&addr, &len);
        if(fd <= 0) {
            return;
        } 
        // 达到最大连接数，直接关闭新的连接, 都是 httpCOnn的静态变量
        else if(HttpConn::userCount >= MAX_FD) {
            sendError_(fd, "Server busy!");
            LOG_WARN("Clients is full!");
            return;
        }
        addClient_(fd, addr);
    } while(listenEvent_ & EPOLLET);
}


void webServer::dealRead_(HttpConn* client) {
    assert(client);
    extTimer_(client);
    threadpool_->addTask(std::bind(&webServer::onRead_, this, client));
}

void webServer::dealWrite_(HttpConn* client) {
    assert(client);
    extTimer_(client);
    threadpool_->addTask(std::bind(&webServer::onWrite_, this, client));
}

void webServer::extTimer_(HttpConn* client){
    assert(client);
    if(timeoutMS_ >0){
        timer_->adjust(client->getFd(),timeoutMS_);
    }
}

void webServer::onRead_(HttpConn* client) {
    assert(client);
    int ret = -1;
    int readErrno = 0;
    ret = client->read(&readErrno);
    if(ret <= 0 && readErrno != EAGAIN) {
        closeConn_(client);
        return;
    }
    onProcess(client);
}

void webServer::onProcess(HttpConn* client) {
    // 处理报文，并接受响应
    if(client->process()) {
        // 读完了，改为写事件
        epoller_->modFd(client->getFd(), connEvent_ | EPOLLOUT);
    } else {
        // 反之还是读事件
        epoller_->modFd(client->getFd(), connEvent_ | EPOLLIN);
    }
}

void webServer::onWrite_(HttpConn* client) {
    assert(client);
    int ret = -1;
    int writeErrno = 0;
    ret = client->write(&writeErrno);
    if(client->ToWriteBytes() == 0) {
        // 已经发送完毕
        if(client->isKeepAlive()) {
            // 写完了，转为监听，就是读事件
            onProcess(client);
            return;
        }
    } else if(ret < 0) {
        if(writeErrno == EAGAIN) {
            // 继续发送
            epoller_->modFd(client->getFd(), connEvent_ | EPOLLOUT);
            return;
        }
    }
    closeConn_(client);
}

// 创建监听fd,只创建一个，后续的由系统在服务器接受连接请求时，自动创建的
bool webServer::initSocket_(){
    int ret;
    struct sockaddr_in addr;
    if(port_ > 65535 || port_ < 1024){
        LOG_ERROR("Port: %d error!", port_);
        return false;
    }
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port_);

    // 优雅关闭，服务器在关闭前确保所有的请求都得到处理
    // 然后再关闭连接或停止服务的过程，
    //这样可以避免数据丢失、错误响应或客户端的连接中断
    {
        struct linger optLinger = {0};
        if(openLinger_){
            optLinger.l_onoff = 1;  // 开启优雅关闭
            optLinger.l_linger = 1;  // 超时时间设置为 1s
        }
    
        listenFd_ = socket(AF_INET,SOCK_STREAM ,0);
        if(listenFd_ < 0){
            LOG_ERROR("create socket error!,port is %d", port_);
            return false;
        }
        ret = setsockopt(listenFd_,SOL_SOCKET,SO_LINGER,&optLinger,sizeof(optLinger));
        if(ret < 0){
            close(listenFd_);
            LOG_ERROR("init linger error!,port is %d", port_);
            return false;
        }
    }
    int optVal = 1;
    // 端口复用，只有最后一个套接字会正常接受数据
    // 设置socket选项，允许端口复用
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, (const void*)&optVal, sizeof(int));
    if(ret == -1){
        LOG_ERROR("set socket setsockopt error !");
        close(listenFd_);
        return false;
    }

    // 绑定
    ret = bind(listenFd_,(struct sockaddr*)&addr,sizeof(addr));
    if(ret < 0){
        LOG_ERROR("Bind Port:%d error!", port_);
        close(listenFd_);
        return false;
    }

    // 监听
    ret = listen(listenFd_,6);  // 最多有6个连接同时等待接受
    if(ret < 0){
        LOG_ERROR("Listen port:%d error!", port_);
        close(listenFd_);
        return false;   
    }

    ret = epoller_-> addFd(listenFd_,listenEvent_|EPOLLIN); // 加入epoller
    if(ret == 0){
        LOG_ERROR("Add listen error!");
        close(listenFd_);
        return false;
    }
    setFdNonblock(listenFd_);
    LOG_INFO("server port:%d",port_);
    return true;
}

// 设置非阻塞
int webServer::setFdNonblock(int fd){
    assert(fd > 0);
    return fcntl(fd,F_SETFL,fcntl(fd,F_GETFD,0) | O_NONBLOCK);
}
