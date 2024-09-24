#include <sys/socket.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/poll.h>
#include <sys/epoll.h>

#define BUFFER_LENGTH 1024

typedef int (*RCALLBACK)(int fd);

// 对于listenfd  → accept_cb()
int acceprt_cb(int fd);

// 对于clientfd → recv_cb()

//                send_cb()

int recv_cb(int fd);

int send_cb(int fd);


struct conn_item{
    int fd;
    char rbuffer[BUFFER_LENGTH];
	int rlen;
	char wbuffer[BUFFER_LENGTH];
	int wlen;

    union {
		RCALLBACK accept_callback;
		RCALLBACK recv_callback;
	} recv_t;
	RCALLBACK send_callback;
};  

int epfd = 0;

struct conn_item conn_list[1024] = {0}; //连接列表

#if 0
struct reactor{
    int epollfd; //epoll实例
    struct conn_item *conn_list; //连接列表
};


#endif

int set_event(int fd, int event,int flag){
    if(flag){   // 1 add,0 modify
        struct epoll_event ev; //epoll事件
        ev.events = event; //监听读事件
        ev.data.fd = fd; //监听clientfd
        epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev); //将clientfd加入epoll实例
    }else{
        struct epoll_event ev; 
        ev.events = event; 
        ev.data.fd = fd; 
        epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev); 

    }
}

int accept_cb(int fd){
    struct sockaddr_in clientaddr;  //客户端地址
    socklen_t len = sizeof(clientaddr); //客户端地址长度

    int clientfd = accept(fd, (struct sockaddr *)&clientaddr, &len); //接受连接
    if(clientfd < 0){
        perror("accept");
        return -1;
    }
    
    set_event(clientfd, EPOLLIN, 1); //将clientfd加入epoll实例，监听读事件

    conn_list[clientfd].fd = clientfd; //将clientfd加入连接列表
    memset(conn_list[clientfd].rbuffer, 0, BUFFER_LENGTH); //清空缓冲区
    conn_list[clientfd].rlen = 0; //缓冲区索引从0开始
    memset(conn_list[clientfd].wbuffer, 0, BUFFER_LENGTH);
	conn_list[clientfd].wlen = 0;

    conn_list[clientfd].recv_t.recv_callback = recv_cb;
    conn_list[clientfd].send_callback = send_cb;

    printf("new client fd %d\n", clientfd);
    return clientfd;
}


// recv
// buffer -> conn_list[fd].buffer
int recv_cb(int fd){
    char* buffer = conn_list[fd].rbuffer; //获取缓冲区
    int idx = conn_list[fd].rlen; //获取缓冲区索引

    int count = recv(fd, buffer+idx, BUFFER_LENGTH-idx,0); //读取数据

    if(count == 0){ //如果读取到0，表示客户端关闭连接
        printf("client close\n");

        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL); //将clientfd从epoll实例中删除
        close(fd); //关闭clientfd

        return -1;
    }
    conn_list[fd].rlen += count; //更新缓冲区索引   
    
#if 1   //need to send
    memcpy(conn_list[fd].wbuffer, conn_list[fd].rbuffer, conn_list[fd].rlen);
    conn_list[fd].wlen = conn_list[fd].rlen;
    conn_list[fd].rlen -= conn_list[fd].wlen;
#endif

    // 修改事件
    set_event(fd,EPOLLOUT,0);
    return count;
}

int send_cb(int fd){
    char* buffer = conn_list[fd].wbuffer; //获取缓冲区
    int idx = conn_list[fd].wlen; //获取缓冲区索引
    int count = send(fd, buffer, idx, 0);

    // 修改事件
    set_event(fd,EPOLLIN,0);

    return count;
}

// tcp 
int main() {

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);   //创建套接字
    
	struct sockaddr_in serveraddr; 
	memset(&serveraddr, 0, sizeof(struct sockaddr_in)); 

	serveraddr.sin_family = AF_INET; 
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY); 
	serveraddr.sin_port = htons(2048);

	if (-1 == bind(sockfd, (struct sockaddr*)&serveraddr, sizeof(struct sockaddr))) { //绑定
		perror("bind");
		return -1;
	}

	listen(sockfd, 10); //监听

//epoll
    int epfd = epoll_create(1); //创建epoll实例

    set_event(sockfd, EPOLLIN, 1);

    struct epoll_event events[1024] = {0}; //epoll事件数组

    while(1){   // mainloop
        int nready = epoll_wait(epfd,events,1024,-1); //等待事件发生

        int i = 0;
        for (i = 0;i < nready;i ++) {

			int connfd = events[i].data.fd;
			if (events[i].events & EPOLLIN) { //

				int count = conn_list[connfd].recv_t.recv_callback(connfd);
				printf("recv count: %d <-- buffer: %s\n", count, conn_list[connfd].rbuffer);

			} else if (events[i].events & EPOLLOUT) { 
				printf("send --> buffer: %s\n",  conn_list[connfd].wbuffer);
				
				int count = conn_list[connfd].send_callback(connfd);
			}

		}

    }

    getchar();
   // close(clientfd);
	
}