/*
muduo 网络库给用户提供了两个类
TcpServer ： 用于编写服务器程序的
TcpClient : 用于编写客户端程序的

epoll +线程池
好处：能够把网络IO的代码和业务处理代码分开
                    用户的连接与断开   用户的可读写事件
*/
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpServer.h>
#include <iostream>
#include <functional>
#include <string>
using namespace std;
using namespace muduo;
using namespace muduo::net;

/*
基于muduo网络库开发服务器程序
1、组合TcpServer对象
2、创建EventLoop事件循环对象的指针
3、明确TcpServer需要哪些参数
4、在当前服务器类的构造函数当中，注册处理连接的回调函数和处理读写事件的回调函数
5、设置合适的服务端线程数量，muduo会自动分配IO线程和worker线程
*/

class MyServer{
public:
    MyServer(EventLoop* loop,     // 事件循环
            const InetAddress& listenAddr,  // IP + port
            const string& nameArg)  // 服务器名称
        :_server(loop, listenAddr, nameArg),_loop(loop)  
            {
                // 给服务器注册用户连接的创建和断开回调函数
                _server.setConnectionCallback(std::bind(&MyServer::onConnection, this, placeholders::_1));
                // 给服务器注册用户读写事件的回调函数
                _server.setMessageCallback(std::bind(&MyServer::onMessage, this, placeholders::_1, placeholders::_2, placeholders::_3));

                //设置线程数量
                _server.setThreadNum(4);    //1个 IO线程 + 3个worker线程
            }
    // 开启事件循环
    void start(){
        _server.start();
    }
private:
    // 专门处理用户连接的回调函数 epoll listenfd accept
    void onConnection(const TcpConnectionPtr& conn)
    { 
        if (conn->connected()) {
            cout << conn->peerAddress().toIpPort() << " -> " << 
                conn->localAddress().toIpPort() << "state:online" << endl;
        }else{
            cout << conn->peerAddress().toIpPort() << " -> " << 
                conn->localAddress().toIpPort() << "state:offline" << endl;
            conn->shutdown();
            // _loop->quit();
        }
    }

    // 专门处理用户读写事件的回调函数 epoll connfd read/write
    void onMessage(const TcpConnectionPtr& conn,    // 连接
                    Buffer* buf,    // 缓冲区
                    Timestamp time)   // 时间戳
    { 
        string msg = buf->retrieveAllAsString();
        cout << "recv date:" << msg << "time" << time.toString() << endl;
        conn->send(msg);
    }

    
    TcpServer _server;  //#1
    EventLoop *_loop;  //#2 epoll
};

int main()
{
    EventLoop loop; //epoll
    InetAddress addr("127.0.0.1",6000);
    MyServer server(&loop,addr,"server");

    server.start(); // listenfd epoll_ctl -> epoll
    loop.loop();    //epoll wait以阻塞方式等待新用户连接，已连接用户的读写事件等；
    
    return 0;
}