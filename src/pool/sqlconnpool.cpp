#include "sqlconnpool.h"

SqlConnPool* SqlConnPool::instance(){
    static SqlConnPool pool;
    return &pool;
}

void SqlConnPool::init(const char* host, int port,
              const char* user,const char* pwd, 
              const char* dbName, int connSize = 10){
    assert(connSize>0);
    for(int i=0; i<connSize;i++){
        MYSQL* conn= nullptr;
        conn = mysql_init(conn);
        if(!conn){
            LOG_ERROR("mysql init %s","error");
            assert(conn);
        }
        conn = mysql_real_connect(conn, host, user, pwd, dbName, port, nullptr, 0);
        if(!conn){
            LOG_ERROR("mysql connect %s","error");  // 不知道可变参数哪里存在问题，因此改了一下
            assert(conn);
        }
        m_connQueue.emplace(conn);
    }
    m_maxConn = connSize;
    sem_init(&m_semID,0,m_maxConn); // 初始化信号量，用于控制对连接池的访问,初始信号量为10，0代表为进程内信号量
}

MYSQL* SqlConnPool::getConn(){
    MYSQL* conn = nullptr;
    if(m_connQueue.empty()){
        LOG_WARN("Sql busy%s","-------");
        return nullptr;
    }
    sem_wait(&m_semID);   // 等待信号量，信号量-1，释放了之后，信号量+1,所以如果信号值为0，就是-1，就会堵塞
    std::lock_guard<std::mutex> locker(m_mtx);  // 保护 m_connQueue的
    conn = m_connQueue.front();
    m_connQueue.pop();
    return conn;
}

// 存入连接池，但是没有关闭
void SqlConnPool::freeConn(MYSQL* conn){
    assert(conn);
    std::lock_guard<std::mutex> locker(m_mtx); 
    m_connQueue.push(conn);
    sem_post(&m_semID);
}

void SqlConnPool::closePool(){
    std::lock_guard<std::mutex> locker(m_mtx);
    while(!m_connQueue.empty()){
        auto conn = m_connQueue.front();
        m_connQueue.pop();
        mysql_close(conn);
    }
    mysql_library_end();
}

int SqlConnPool::getFreeConnCount(){
    std::lock_guard<std::mutex> locker(m_mtx);
    return m_connQueue.size();
}

