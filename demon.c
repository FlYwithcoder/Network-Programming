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

#if 0
	struct sockaddr_in clientaddr;
	socklen_t len = sizeof(clientaddr);
	int clientfd = accept(sockfd, (struct sockaddr*)&clientaddr, &len);
    printf("accept:\n");

#if 0
    char buffer[128] = {0};
    int count = recv(clientfd, buffer, sizeof(buffer), 0);
    send(clientfd,buffer,count,0);
    printf("sockfd: %d, clientfd = %d\n count = %d\n", sockfd, clientfd, count);

    while(1){
        char buffer[128] = {0};
        int count = recv(clientfd, buffer, sizeof(buffer), 0);
        if(count == 0){ //recv返回0 的时候是对端调用close了
            break;
        }
        send(clientfd,buffer,count,0);
        printf("sockfd: %d, clientfd = %d\n count = %d\n", sockfd, clientfd, count);
    }
#endif  
#elseif 0

    while(1){
        struct sockaddr_in clientaddr;
        socklen_t len = sizeof(clientaddr);
        int clientfd = accept(sockfd, (struct sockaddr*)&clientaddr, &len);

        pthread_t thid;
        pthread_create(&thid, NULL, client_thread, &clientfd);  //client_thread函数中需要传入唯一参数clientfd

        printf("accept:\n");
    }

#elseif 0
   //select  （有事件(可读可写，异常)就返回，否则就等timeout返回）
    //  int nready = select(maxfd, rset, wset, eset, timeout);
    // maxfd主要用于select内部循环
    // rset, wset, eset用于判断fd是否可读(使用数组 bit)，可写，异常
    // timeout用于设置select的超时时间

    // typedef struct
    // {
    //     unsigned long fds_bits[1024 / (8 * sizeof(long))];
    // } fd_set;

    fd_set rfds, rset;

    FD_ZERO(&rfds); //清空集合
    FD_SET(sockfd, &rfds); //将sockfd加入集合

    int maxfd = sockfd;

    printf("loop \n");
    while(1){
        rset = rfds;
        
        int nready = select(maxfd+1, &rset, NULL, NULL, NULL); //阻塞等待sockfd可读

        if(FD_ISSET(sockfd, &rset)){ //判断sockfd是否可读
            struct sockaddr_in clientaddr;
            socklen_t len = sizeof(clientaddr);
            int clientfd = accept(sockfd, (struct sockaddr*)&clientaddr, &len);
            printf("socket: %d\n", clientfd);

            FD_SET(clientfd, &rfds); //将clientfd加入集合
            maxfd = clientfd; //更新maxfd
        }
        for(int i= sockfd+1; i<=maxfd; i++){
            if(FD_ISSET(i, &rset)){
                char buffer[128] = {0};
                int count = recv(i, buffer, sizeof(buffer), 0);
                if(count == 0){
                    printf("client close\n");
                    FD_CLR(i, &rfds); //将clientfd从集合中清除
                    close(i);
                    break;
                }
                send(i, buffer, count, 0);
                printf("clientfd: %d, count: %d, buffer: %s\n",i, count, buffer);
            }
        }
    }

#elseif 0   //poll
 
    struct pollfd fds[1024] = {0};
    fds[sockfd].fd = sockfd;
    fds[sockfd].events = POLLIN;    //POLLIN 为可读，POLLOUT为可写

    int maxfd = sockfd;

    while(1){
        int nready = poll(fds, maxfd+1, -1);    //阻塞等待

        if(fds[sockfd].revents & POLLIN){
            struct sockaddr_in clientaddr;
            socklen_t len = sizeof(clientaddr);

            int clientfd = accept(sockfd, (struct sockaddr*)&clientaddr, &len);
            printf("socket: %d\n", clientfd);
            
            fds[clientfd].fd = clientfd;    
            fds[clientfd].events = POLLIN; 

            if(clientfd > maxfd){   //更新最大文件描述符
                maxfd = clientfd;
            }
        }

        for(int i = sockfd+1; i <= maxfd; i++){
            if(fds[i].revents & POLLIN){
                char buffer[1024] = {0};
                int count = recv(i, buffer, sizeof(buffer), 0);
                if(count == 0){
                    printf("client close\n");
                    fds[i].fd = -1;  //标记该socket关闭
                    fds[i].events = 0; // 清空该socket的事件
                    close(i);
                    continue;
                }
                send(i,buffer,count,0); 
                printf("clientfd: %d, count: %d, buffer: %s\n", i, count, buffer);
            }
        }
    }


#else   //epoll
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

            if(connfd == sockfd){ //如果是监听socket，表示有新的连接
                struct sockaddr_in clientaddr;  //客户端地址
                socklen_t len = sizeof(clientaddr); //客户端地址长度

                int clientfd = accept(sockfd, (struct sockaddr *)&clientaddr, &len); //接受连接

                ev.events = EPOLLIN; //监听读事件
                ev.data.fd = clientfd; //监听clientfd

                epoll_ctl(epfd, EPOLL_CTL_ADD, clientfd, &ev); //将clientfd加入epoll实例
                printf("clientfd: %d\n", clientfd);
            }else if(events[i].events & EPOLLIN){ //如果是读事件
                char buf[1024] = {0}; //缓冲区

                int count = recv(connfd, buf, sizeof(buf),0); //读取数据

                if(count == 0){ //如果读取到0，表示客户端关闭连接
                    printf("client close\n");

                    epoll_ctl(epfd, EPOLL_CTL_DEL, connfd, NULL); //将clientfd从epoll实例中删除
                    close(connfd); //关闭clientfd

                    continue;
                }
                send(connfd, buf, count, 0); //将数据写回客户端
                printf("clientfd: %d,count: %d, buffer: %s\n", connfd, count, buf);
            }
        }
    }



#endif

    getchar();
   // close(clientfd);
	
   
}