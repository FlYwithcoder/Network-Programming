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

#define buffer_length 128

struct conn_item{
    int fd;
    char buffer[buffer_length];
    int idx; //buffer的索引，记录已经读到的位置
};

struct conn_item conn_list[1024] = {0}; //连接列表

// block
void* client_thread(void* arg){
    int clientfd = *(int *)arg;   //
    while(1){
        char buffer[128] = {0};
        int count = recv(clientfd, buffer, sizeof(buffer), 0);
        if(count == 0){     //recv返回0 的时候是对端调用close了
            break;
        }
        send(clientfd,buffer,count,0);
        printf("clientfd: %d, count: %d, buffer: %s\n", clientfd, count, buffer);
    }
    
    close(clientfd);
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

    struct epoll_event ev; //epoll事件
    ev.events = EPOLLIN; //监听读事件
    ev.data.fd = sockfd; //监听sockfd

    epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &ev); //将sockfd加入epoll实例

    struct epoll_event events[1024]; //epoll事件数组

    while(1){
        int nready = epoll_wait(epfd,events,1024,-1); //等待事件发生

        for(int i = 0; i < nready; i++){
            int connfd = events[i].data.fd; //获取发生事件的socket

            if(sockfd == connfd){ //如果是监听socket，表示有新的连接

                struct sockaddr_in clientaddr;  //客户端地址
                socklen_t len = sizeof(clientaddr); //客户端地址长度

                int clientfd = accept(sockfd, (struct sockaddr *)&clientaddr, &len); //接受连接
                ev.events = EPOLLIN; //监听读事件
                ev.data.fd = clientfd; //监听clientfd
                epoll_ctl(epfd, EPOLL_CTL_ADD, clientfd, &ev); //将clientfd加入epoll实例

                conn_list[clientfd].fd = clientfd; //将clientfd加入连接列表
                memset(conn_list[clientfd].buffer, 0, buffer_length); //清空缓冲区
                conn_list[clientfd].idx = 0; //缓冲区索引从0开始

                printf("clientfd: %d\n", clientfd);
            }else if(events[i].events & EPOLLIN){ //如果是读事件
                char* buffer = conn_list[connfd].buffer; //获取缓冲区
                int idx = conn_list[connfd].idx; //获取缓冲区索引

                int count = recv(connfd, buffer+idx, buffer_length-idx,0); //读取数据

                if(count == 0){ //如果读取到0，表示客户端关闭连接
                    printf("client close\n");

                    epoll_ctl(epfd, EPOLL_CTL_DEL, connfd, NULL); //将clientfd从epoll实例中删除
                    close(connfd); //关闭clientfd

                    continue;
                }
                conn_list[connfd].idx += count; //更新缓冲区索引    

                send(connfd, buffer, count, 0); //将数据写回客户端
                printf("clientfd: %d,count: %d, buffer: %s\n", connfd, count, buffer);
            }
        }

    }

    getchar();
   // close(clientfd);
	
}