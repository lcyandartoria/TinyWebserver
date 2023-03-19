/* TinyWebserver */
/* 利用Socket来实现不同主机之间的通信。
 * 使用epoll技术实现I/O多路复用，提高效率。
 * 使用有限状态机高校解析HTTP报文，可对GET和POST请求进行处理，这个服务器中我没有拓展对携带数据的请求进一步分析的功能。
 * 利用多线程机制提供服务，增加并行服务数量。
 * 可以访问服务端相关文件（mp3、图片、文件目录、视频等内容）
 */
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <signal.h>
#include <dirent.h>
#include <pthread.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "fengzhuang.h"
typedef int CHECK_STATE;
typedef int LINE_STATUS;
typedef int HTTP_CODE;

/* 读缓冲区大小。 */
#define BUFFER_SIZE 4096 

/* 主状态机的两种可能状态。
 * 分别表示：当前正在分析请求行，当前正在分析头部字段。
 */
#define CHECK_STATE_REQUESTLINE 0
#define CHECK_STATE_HEADER 1

/* 从状态机的三种可能状态。
 * 即行的读取状态，分别表示：
 * 读取到一个完整的行、行出错和行数据尚且不完整。
 */
#define LINE_OK 0
#define LINE_BAD 1
#define LINE_OPEN 2

/* 服务器处理HTTP请求的结果：
 * NO_REQUEST表示请求不完整，需要继续读取客户数据；
 * GET_REQUEST表示获得了一个完整的客户请求；
 * BAD_REQUEST表示客户请求有错误。
 */
#define NO_REQUEST 0
#define GET_REQUEST 1
#define BAD_REQUEST 2

struct ep
{
	int epfd;
	int cfd;
};

/* 发送头部信息。*/
int send_header(int cfd, char *code, char *msg, char *fileType, int len);

/* 发送文件内容。 */
int send_file(int cfd, char *fileName);

/* 从状态机，用于解析出一行内容。 */
LINE_STATUS parse_line( char* buffer, int* checked_index, int* read_index )
{
    char temp;
	/* checked_index 指向buffer（应用程序的读缓冲区）中当前正在分析的字节，read_index指向buffer中客户数据的尾部的下一字节.
     * buffer中第0~checked_index字节都已经分析完毕，第checked_index~（read_index-1)字节由下面的循环挨个分析
     */
    for ( ; (*checked_index) < (*read_index); ++(*checked_index) )
    {
        temp = buffer[ (*checked_index) ];
		/* 如果当前字节是"\r",即回车符，则说明可能读到一个完整的行 */
        if ( temp == '\r' )
        {
			/* 如果当前是本行最后一个字符，则说明不完整，需要更多数据
             * 如果下一个字符是'\n'则说明读取到了完整的行
             * 否则说明HTTP请求存在语法问题
             */
            if ( ( (*checked_index) + 1 ) == (*read_index) )
            {
                return LINE_OPEN;
            }
            else if ( buffer[ (*checked_index) + 1 ] == '\n' )
            {
                buffer[ (*checked_index)++ ] = '\0';
                buffer[ (*checked_index)++ ] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
		/* 如果当前的字符是'\n'，即换行符，则也说明可能读取到一个完整的行 */
        else if( temp == '\n' )
        {
            if( ( (*checked_index) > 1 ) &&  buffer[ (*checked_index) - 1 ] == '\r' )
            {
                buffer[ (*checked_index)-1 ] = '\0';
                buffer[ (*checked_index)++ ] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
	/* 如果所有内容都分析完毕也没有遇到'\r'字符，则返回LINE_OPEN，表示还需要继续读取客户数据才能进一步分析 */
    return LINE_OPEN;
}

/* 分析请求行 */
HTTP_CODE parse_requestline( char* szTemp, CHECK_STATE *checkstate,int cfd )
{
	/* strpbrk:依次检验字符串s1中的字符，当被检验字符在字符串s2中也包含时，则停止检验，并返回该字符位置，空字符null不包括在内。 */
    char* szURL = strpbrk( szTemp, " \t" );
    if ( ! szURL )
    {
        return BAD_REQUEST;
    }
    *szURL++ = '\0';

    char* szMethod = szTemp;
    if ( strcasecmp( szMethod, "GET" ) == 0 )
    {
        printf( "The request method is GET\n" );
    }
    else if(strcasecmp( szMethod, "POST" ) == 0)
    {
		printf( "The request method is POST\n" );
    }
	else
	{
		return BAD_REQUEST;
	}
	/*就是从str1的第一个元素开始往后数，看str1中是不是连续往后每个字符都在str2中可以找到。到第一个不在str2的元素为止。看从str1第一个开始，前面的字符有几个在str2中。 */
    szURL += strspn( szURL, " \t" );
    char* szVersion = strpbrk( szURL, " \t" );
    if ( ! szVersion )
    {
        return BAD_REQUEST;
    }
    *szVersion++ = '\0';
	/* 仅支持HTTP/1.1 */
    szVersion += strspn( szVersion, " \t" );
    if ( strcasecmp( szVersion, "HTTP/1.1" ) != 0 )
    {
        return BAD_REQUEST;
    }
	/* 检查URL是否合法 */
    if ( strncasecmp( szURL, "http://", 7 ) == 0 )
    {
        szURL += 7;
        szURL = strchr( szURL, '/' );/* 首次出现'/'的位置 */
    }

    if ( ! szURL || szURL[ 0 ] != '/' )
    {
        return BAD_REQUEST;
    }

    /*URLDecode( szURL );*/
    printf( "The request URL is: %s\n", szURL );
	
	char *pFile = szURL;/*不能为空，要有合法指向*/
	if(strlen(szURL)<=1)
	{
		strcpy(pFile, "./");
	}
	else 
	{
		pFile = szURL+1;
	}
	
	/*转换汉字编码*/
	strdecode(pFile, pFile);
	/*判断文件是否存在*/
	struct stat st;
	if(stat(pFile, &st)<0)
	{
		printf("file not exist\n");
		
		send_header(cfd, "404", "NOT FOUND", get_mime_type(".html"), 0);
		
		send_file(cfd, "error.html");
		return BAD_REQUEST;	
	}
	else /*若文件存在*/
	{
		/*判断文件类型*/
		/*普通文件*/
		if(S_ISREG(st.st_mode))
		{
			printf("file exist\n");
			/*发送头部信息*/
			send_header(cfd, "200", "OK", get_mime_type(pFile), st.st_size);
			
			/*发送文件内容*/
			send_file(cfd, pFile);
		}
		/*目录文件
		 * 十个目录下文件名一组发送*/
		else if(S_ISDIR(st.st_mode))
		{
			//printf("目录文件\n");
			
			char buffer[1024];
			char temp[1024];
			/*发送头部信息*/
			send_header(cfd, "200", "OK", get_mime_type(".html"), 0);	
			
			/*发送html文件头部*/
			send_file(cfd, "html/dir_header.html");	
			
			/*文件列表信息*/
			struct dirent **namelist;
			int num;

			num = scandir(pFile, &namelist, NULL, alphasort);
			if (num < 0)
			{
			   return BAD_REQUEST;
			   
			}
			else 
			{
			   int k=0;
			   while (num--) 
			   {
			       //printf("%s\n", namelist[num]->d_name);
			       if(++k==11)
			       {
					  Write(cfd,buffer,strlen(buffer));
					  memset(buffer, 0x00, sizeof(buffer));
					  k=1;
			       }
			       memset(temp, 0x00, sizeof(temp));
			       if(namelist[num]->d_type==DT_DIR)
			       {
					  sprintf(temp, "<li><a href=%s/>%s</a></li>", namelist[num]->d_name, namelist[num]->d_name);
			       }
			       else
			       {
					  sprintf(temp, "<li><a href=%s>%s</a></li>", namelist[num]->d_name, namelist[num]->d_name);
			       }
			       strcat(buffer,temp);
			       strcat(buffer,"\n");
			       free(namelist[num]);
			   }
			   if(buffer[0]!='\0')
			   {
			       Write(cfd,buffer,strlen(buffer));
			       memset(buffer, 0x00, sizeof(buffer));
			   }
			   free(namelist);
			}
			/*发送html尾部*/
			send_file(cfd, "html/dir_tail.html");		
		}
	}
	/* HTTP 请求行处理完毕，状态转移到头部字段的分析 */
    *checkstate = CHECK_STATE_HEADER;
	/* 表示请求不完整，需要继续读取客户端数据； */
    return NO_REQUEST;
}

/* 分析头部字段 */
HTTP_CODE parse_headers( char* szTemp )
{
	/* 遇到一个空行，说明我们得到了一个正确的HTTP请求 */
    if ( szTemp[ 0 ] == '\0' )
    {
        return GET_REQUEST;
    }
    else if ( strncasecmp( szTemp, "Host:", 5 ) == 0 )/* 处理“HOST”头部字段 */
    {
        szTemp += 5;
        szTemp += strspn( szTemp, " \t" );
        printf( "the request host is: %s\n", szTemp );
    }
    else/* 其他头部字段都不处理 */
    {
        //printf( "I can not handle this header\n" );
    }

    return NO_REQUEST;
}

/* 分析HTTP请求的入口函数 */
HTTP_CODE parse_content( char* buffer, int* checked_index, CHECK_STATE* checkstate, int* read_index, int* start_line,int cfd)
{
    LINE_STATUS linestatus = LINE_OK;/* 记录当前行的读取状态 */
    HTTP_CODE retcode = NO_REQUEST;/* 记录HTTP请求的处理结果 */
    while( ( linestatus = parse_line( buffer, checked_index, read_index ) ) == LINE_OK )
    {
        char* szTemp = buffer + *start_line;
        *start_line = *checked_index;
        switch ( *checkstate )
        {
            case CHECK_STATE_REQUESTLINE: /* 第一个状态 */
            {
                retcode = parse_requestline( szTemp, checkstate,cfd );
                if ( retcode == BAD_REQUEST )
                {
                    return BAD_REQUEST;
                }
                break;
            }
            case CHECK_STATE_HEADER:/* 第二个状态 */
            {
                retcode = parse_headers( szTemp );
                if ( retcode == BAD_REQUEST )
                {
                    return BAD_REQUEST;
                }
                else if ( retcode == GET_REQUEST )
                {
                    printf("*****************************\n");
                    return GET_REQUEST;
                }
                break;
            }
            default:
            {
                return BAD_REQUEST;
            }
        }
    }
    if( linestatus == LINE_OPEN )/* 若没有读取到一个完整的行，则表示还需要继续读取客户数据才能进一步分析 */
    {
        return NO_REQUEST;
    }
    else
    {
        return BAD_REQUEST;
    }
}

/*工作线程，用于接受用户请求并响应*/
void *http_request(void *arg)
{
	struct ep node=*(struct ep*)arg;
	int epfd=node.epfd;
	int cfd=node.cfd;
	char buffer[ BUFFER_SIZE ]; /* 读缓冲区 */
	memset( buffer, '\0', BUFFER_SIZE );
	int data_read = 0;
	int read_index = 0;/* 当前已经读取了多少字节的客户数据 */
	int checked_index = 0; /* 当前已经分析完了多少字节的客户数据 */
	int start_line = 0; /* 行在buffer中的起始位置 */
	
	/* 设置主状态机的初始状态 */
	CHECK_STATE checkstate = CHECK_STATE_REQUESTLINE;
	while( 1 )
	{
		data_read = recv( cfd, buffer + read_index, BUFFER_SIZE - read_index, 0 );
		if ( data_read <0)
		{
			if( errno == EAGAIN )
			{
			 	reset_oneshot( epfd, cfd );
			} 
			break;
		}
		else if ( data_read == 0 )
		{
			/*关闭连接*/
			close(cfd);
			printf("Down the tree\n");
			/*将文件描述符从epoll树上删除*/
			epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL);
			printf( "remote client has closed the connection\n" );
			break;
		}

		read_index += data_read;
		/* 分析目前已获得的所有客户数据 */
		HTTP_CODE result = parse_content( buffer, &checked_index, &checkstate, &read_index, &start_line,cfd );
		if( result == NO_REQUEST)/* 尚未得到一个完整的HTTP请求 */
		{
			continue;
		}
		else if(result == GET_REQUEST)/* 得到一个完整的、正确的HTTP请求 */
		{
			while(data_read = recv( cfd, buffer , BUFFER_SIZE , 0 )>0);
			/*关闭连接*/
			close(cfd);
			/*将文件描述符从epoll树上删除*/
			epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL);
			printf( "remote client has closed the connection\n" );
			break;
		}
		else /* 其他情况均发生错误 */
		{	
			/*关闭连接*/
			close(cfd);
			printf("BAD_REQUEST\n");
			/*将文件描述符从epoll树上删除*/
			epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL);
			printf( "remote client has closed the connection\n" );
			break;
		}
	}

	pthread_exit(NULL);
}

/*主函数，主要用于监听客户端请求 */
int main()
{
	/*若web服务器给浏览器发送数据的时候, 浏览器已经关闭连接, 
	 *则web服务器就会收到SIGPIPE信号*/
	struct sigaction act;
	/*忽略SIGPIPE信号*/
	act.sa_handler = SIG_IGN;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	sigaction(SIGPIPE, &act, NULL);
	
	/*改变当前进程的工作目录*/
	char path[255] = {0};
	sprintf(path, "%s/%s", getenv("HOME"), "webpath");
	chdir(path);
	
	/*创建socket--设置端口复用---bind*/
	int lfd = Tcp3bind(9999, NULL);
	
	/*设置监听*/
	Listen(lfd, 128);

	/*创建epoll树*/
	int epfd = epoll_create(1024);
	if(epfd<0)
	{
		perror("epoll_create error");
		close(lfd);
		return -1;
	}
	
	/*将监听文件描述符lfd上树*/
	struct epoll_event ev;
	ev.data.fd = lfd;
	ev.events = EPOLLIN;
	epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &ev);
	
	int i;
	int cfd;
	int nready;
	int sockfd;
	struct epoll_event events[1024];
	while(1)
	{
		/*等待事件发生*/
		nready = epoll_wait(epfd, events, 1024, -1);
		if(nready<0)
		{
			if(errno==EINTR)
			{
				continue;
			}
			break;
		}
		
		for(i=0; i<nready; i++)
		{
			sockfd = events[i].data.fd;
			/*有客户端连接请求*/
			if(sockfd==lfd)
			{
				/*接受新的客户端连接*/
				cfd = Accept(lfd, NULL, NULL);
				/*将新的cfd上树*/
				/*设置cfd为非阻塞*/
				addfd( epfd, cfd );
			}
			else if(events[i].events & EPOLLIN)
			{	
				/*有客户端数据发来*/
				pthread_attr_t attr;
				pthread_t threadId;
				

				/*初始化属性值，均设为默认值*/ 
				pthread_attr_init(&attr); 
				pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM); 
				/* 设置线程为分离属性*/ 
				pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
				struct ep arg;
				arg.epfd=epfd;
				arg.cfd=sockfd;
				
				/*交给线程处理客户请求*/
				if(pthread_create(&threadId,&attr,http_request,(void*)&(arg)))
				{ 
				    perror("pthread_creat error!"); 
				} 
			}			
		}		
	}
	close(epfd);
	close(lfd);
	return 0;
}

int send_header(int cfd, char *code, char *msg, char *fileType, int len)
{
	char buf[1024] = {0};
	sprintf(buf, "HTTP/1.1 %s %s\r\n", code, msg);
	sprintf(buf+strlen(buf), "Content-Type:%s\r\n", fileType);
	if(len>0)
	{
		sprintf(buf+strlen(buf), "Content-Length:%d\r\n", len);
	}
	strcat(buf, "\r\n");
	Write(cfd, buf, strlen(buf));
	return 0;
}

int send_file(int cfd, char *fileName)
{
	/*打开文件*/
	int fd = open(fileName, O_RDONLY);
	if(fd<0)
	{
		printf("Error: %s\n", strerror(errno));
		return -1;
	}
	
	/*循环读文件, 然后发送*/
	int n;
	char buf[1024];
	while(1)
	{
		memset(buf, 0x00, sizeof(buf));
		n = Read(fd, buf, sizeof(buf));
		if(n<=0)
		{
			break;
		}
		else 
		{
			Write(cfd, buf, n);
		}
	}
}
