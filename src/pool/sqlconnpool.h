#ifndef SQLCONNPOOL_H
#define SQLCONNPOOL_H

#include <mysql/mysql.h>
#include <string>
#include <queue>
#include <mutex>
#include <semaphore.h>  //信号量相关头文件
#include <thread>
#include "../log/log.h"

// 连接池
class SqlConnPool{
public:
    static SqlConnPool *instance();
    MYSQL *getConn();   //返回一个已建立的 MySQL 连接句柄
    void freeConn(MYSQL* conn); // 释放一个连接
    int getFreeConnCount(); // 返回可用连接数量

    void init(const char* host, int port,
            const char* user,const char* pwd, 
            const char* dbName, int connSize);   // 初始数据库连接池， host为数据库服务器IP，port为端口，user：用户名 pwd密码 dbName：数据库名字 connSize：连接池大小
    void closePool();   // 关闭连接池
private:
    SqlConnPool() = default;
    ~SqlConnPool(){
        closePool();
    }
    int m_maxConn;
    std::queue<MYSQL*> m_connQueue;
    std::mutex m_mtx;
    sem_t m_semID;
};

class SqlConnRALL{
public:
    SqlConnRALL(MYSQL* sql,SqlConnPool * connPool){
        assert(connPool);
        sql = connPool->getConn();
        m_sql = sql;
        m_connPool = connPool;
    }
    ~SqlConnRALL(){
        if(m_sql){
            m_connPool->freeConn(m_sql);
        }
    }
private:
    MYSQL* m_sql;
    SqlConnPool* m_connPool;
};
#endif // SQLCONNPOOL_H