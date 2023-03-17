/* 这个是各个小函数的实现文件 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <strings.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/epoll.h>

/**********************************************************************/
/* 将输出错误与异常退出封装在一起。*/
/**********************************************************************/
void perr_exit(const char *s)
{
	perror(s);
	
	exit(-1);
}

/**********************************************************************/
/* 封装accept函数，如果因为一些可以忽略的信号错误
 * 导致accept函数不成功，将自动重新调用accept函数。
 * 出错的话会终止程序。 */
/**********************************************************************/
int Accept(int fd, struct sockaddr *client, socklen_t *clientlen)
{
	int n;
	
	while((n = accept(fd, client, clientlen)) < 0)
	{
		if ((errno == ECONNABORTED) || (errno == EINTR))
		{
			continue;
		}
		else
			perr_exit("accept error");
	}
	return n;
}

/**********************************************************************/
/* 封装bind函数，出错的话会终止程序。 */
/**********************************************************************/
int Bind(int fd, const struct sockaddr *server, socklen_t serlen)
{
    int n;

	if ((n = bind(fd, server, serlen)) < 0)
		perr_exit("bind error");

    return n;
}

/**********************************************************************/
/* 封装listen函数，出错的话会终止程序。 */
/**********************************************************************/
int Listen(int fd, int backlog)
{
    int n;

	if ((n = listen(fd, backlog)) < 0)
		perr_exit("listen error");

    return n;
}

/**********************************************************************/
/* 封装socket函数，出错的话会终止程序。 */
/**********************************************************************/
int Socket(int family, int type, int protocol)
{
	int n;

	if ((n = socket(family, type, protocol)) < 0)
		perr_exit("socket error");

	return n;
}

/**********************************************************************/
/* 封装read函数，如果因为一些可以忽略的信号错误
 * 导致read函数不成功，将自动重新调用read函数。
 * 出错的话会结束函数。 */
/**********************************************************************/
ssize_t Read(int fd, void *ptr, size_t nbytes)
{
	ssize_t n;
	
	while( (n = read(fd, ptr, nbytes)) == -1)
	{
		if (errno == EINTR)
			continue;
		else
			return -1;
	}
	
	return n;
}

/**********************************************************************/
/* 封装write函数，如果因为一些可以忽略的信号错误
 * 导致write函数不成功，将自动重新调用write函数。
 * 出错的话会结束函数。 */
/**********************************************************************/
ssize_t Write(int fd, const void *ptr, size_t nbytes)
{
	ssize_t n;
	
	while( (n = write(fd, ptr, nbytes)) == -1)
	{
		if (errno == EINTR)
			continue;
		else
			return -1;
	}
	
	return n;
}

/**********************************************************************/
/* 封装3步必要操作，直接将IP地址和端口号绑定好，
 * 返回绑定好的文件描述符。 */
/**********************************************************************/
int Tcp3bind(short port,const char *IP)
{
    struct sockaddr_in serv_addr;
	
	//创建socket, 得到监听文件描述符lfd----socket()
    int lfd = Socket(AF_INET,SOCK_STREAM,0);
    
	//设置端口复用----setsockopt()
    int opt = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int));
    
	//绑定----bind()
	bzero(&serv_addr,sizeof(serv_addr));
    if(IP == NULL){
        //如果这样使用 0.0.0.0,任意ip将可以连接
        serv_addr.sin_addr.s_addr = INADDR_ANY;
    }else{
        if(inet_pton(AF_INET,IP,&serv_addr.sin_addr.s_addr) <= 0){
            perror(IP);//转换失败
            exit(1);
        }
    }
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port   = htons(port);
    Bind(lfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr));
	
    return lfd;
}

/**********************************************************************/
/* 封装设置文件描述符为非阻塞过程。 */
/**********************************************************************/
void setnonblocking( int fd )
{
    int old_option = fcntl( fd, F_GETFL );
    int new_option = old_option | O_NONBLOCK;
    fcntl( fd, F_SETFL, new_option );
	
    return;
}

/**********************************************************************/
/* 封装上epoll树过程。 */
/**********************************************************************/
void addfd( int epollfd, int fd )
{
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;//
    //event.events = EPOLLIN|EPOLLONESHOT;
    epoll_ctl( epollfd, EPOLL_CTL_ADD, fd, &event );
    setnonblocking( fd );
}

/**********************************************************************/
/* 通过文件名字获得文件类型。 */
/**********************************************************************/
char *get_mime_type(char *name)
{
    char* dot;

    dot = strrchr(name, '.');	//自右向左查找‘.’字符，返回 str 中最后一次出现字符 c 的位置;如不存在返回NULL
    /*
     *以下是依据传递进来的文件名，使用后缀判断是何种文件类型
     *将对应的文件类型按照http定义的关键字发送回去
     */
    if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0)
        return "text/html; charset=utf-8";
    if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0)
        return "image/jpeg";
    if (strcmp(dot, ".gif") == 0)
        return "image/gif";
    if (strcmp(dot, ".png") == 0)
        return "image/png";
    if (strcmp(dot, ".css") == 0)
        return "text/css";
    if (strcmp(dot, ".au") == 0)
        return "audio/basic";
    if (strcmp( dot, ".wav") == 0)
        return "audio/wav";
    if (strcmp(dot, ".avi") == 0)
        return "video/x-msvideo";
    if (strcmp(dot, ".mov") == 0 || strcmp(dot, ".qt") == 0)
        return "video/quicktime";
    if (strcmp(dot, ".mpeg") == 0 || strcmp(dot, ".mpe") == 0)
        return "video/mpeg";
    if (strcmp(dot, ".vrml") == 0 || strcmp(dot, ".wrl") == 0)
        return "model/vrml";
    if (strcmp(dot, ".midi") == 0 || strcmp(dot, ".mid") == 0)
        return "audio/midi";
    if (strcmp(dot, ".mp3") == 0)
        return "audio/mpeg";
    if (strcmp(dot, ".ogg") == 0)
        return "application/ogg";
    if (strcmp(dot, ".pac") == 0)
        return "application/x-ns-proxy-autoconfig";

    return "text/plain; charset=utf-8";
}

/**********************************************************************/
/* 16进制数转化为10进制, return 0不会出现。 */
/**********************************************************************/
int hexit(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;

    return 0;
}

/**********************************************************************/
/* 这里的内容是处理%20之类的东西！是"解码"过程。
 * %20 URL编码中的‘ ’(space)
 * %21 '!' %22 '"' %23 '#' %24 '$'
 * %25 '%' %26 '&' %27 ''' %28 '('......
 * 解决不能访问中文文件问题。 */
/**********************************************************************/
void strdecode(char *to, char *from)
{
    for ( ; *from != '\0'; ++to, ++from) {

        if (from[0] == '%' && isxdigit(from[1]) && isxdigit(from[2])) { //依次判断from中 %20 三个字符

            *to = hexit(from[1])*16 + hexit(from[2]);//字符串E8变成了真正的16进制的E8
            from += 2;                      //移过已经处理的两个字符(%21指针指向1),表达式3的++from还会再向后移一个字符
        } else
            *to = *from;
    }
    *to = '\0';
}

/**********************************************************************/
/* 重置fd上的事件。这样操作之后，尽管fd上的EPOLLONESHOT事件
 * 被注册，但是操作系统仍然会触发fd上的EPOLLIN事件，且只会触发一次。 */
/**********************************************************************/
void reset_oneshot( int epollfd, int fd )
{
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
    //event.events = EPOLLIN | EPOLLONESHOT;
    epoll_ctl( epollfd, EPOLL_CTL_MOD, fd, &event );
}
