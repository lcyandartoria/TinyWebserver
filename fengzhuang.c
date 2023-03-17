/* ����Ǹ���С������ʵ���ļ� */
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
/* ������������쳣�˳���װ��һ��*/
/**********************************************************************/
void perr_exit(const char *s)
{
	perror(s);
	
	exit(-1);
}

/**********************************************************************/
/* ��װaccept�����������ΪһЩ���Ժ��Ե��źŴ���
 * ����accept�������ɹ������Զ����µ���accept������
 * ����Ļ�����ֹ���� */
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
/* ��װbind����������Ļ�����ֹ���� */
/**********************************************************************/
int Bind(int fd, const struct sockaddr *server, socklen_t serlen)
{
    int n;

	if ((n = bind(fd, server, serlen)) < 0)
		perr_exit("bind error");

    return n;
}

/**********************************************************************/
/* ��װlisten����������Ļ�����ֹ���� */
/**********************************************************************/
int Listen(int fd, int backlog)
{
    int n;

	if ((n = listen(fd, backlog)) < 0)
		perr_exit("listen error");

    return n;
}

/**********************************************************************/
/* ��װsocket����������Ļ�����ֹ���� */
/**********************************************************************/
int Socket(int family, int type, int protocol)
{
	int n;

	if ((n = socket(family, type, protocol)) < 0)
		perr_exit("socket error");

	return n;
}

/**********************************************************************/
/* ��װread�����������ΪһЩ���Ժ��Ե��źŴ���
 * ����read�������ɹ������Զ����µ���read������
 * ����Ļ������������ */
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
/* ��װwrite�����������ΪһЩ���Ժ��Ե��źŴ���
 * ����write�������ɹ������Զ����µ���write������
 * ����Ļ������������ */
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
/* ��װ3����Ҫ������ֱ�ӽ�IP��ַ�Ͷ˿ںŰ󶨺ã�
 * ���ذ󶨺õ��ļ��������� */
/**********************************************************************/
int Tcp3bind(short port,const char *IP)
{
    struct sockaddr_in serv_addr;
	
	//����socket, �õ������ļ�������lfd----socket()
    int lfd = Socket(AF_INET,SOCK_STREAM,0);
    
	//���ö˿ڸ���----setsockopt()
    int opt = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int));
    
	//��----bind()
	bzero(&serv_addr,sizeof(serv_addr));
    if(IP == NULL){
        //�������ʹ�� 0.0.0.0,����ip����������
        serv_addr.sin_addr.s_addr = INADDR_ANY;
    }else{
        if(inet_pton(AF_INET,IP,&serv_addr.sin_addr.s_addr) <= 0){
            perror(IP);//ת��ʧ��
            exit(1);
        }
    }
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port   = htons(port);
    Bind(lfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr));
	
    return lfd;
}

/**********************************************************************/
/* ��װ�����ļ�������Ϊ���������̡� */
/**********************************************************************/
void setnonblocking( int fd )
{
    int old_option = fcntl( fd, F_GETFL );
    int new_option = old_option | O_NONBLOCK;
    fcntl( fd, F_SETFL, new_option );
	
    return;
}

/**********************************************************************/
/* ��װ��epoll�����̡� */
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
/* ͨ���ļ����ֻ���ļ����͡� */
/**********************************************************************/
char *get_mime_type(char *name)
{
    char* dot;

    dot = strrchr(name, '.');	//����������ҡ�.���ַ������� str �����һ�γ����ַ� c ��λ��;�粻���ڷ���NULL
    /*
     *���������ݴ��ݽ������ļ�����ʹ�ú�׺�ж��Ǻ����ļ�����
     *����Ӧ���ļ����Ͱ���http����Ĺؼ��ַ��ͻ�ȥ
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
/* 16������ת��Ϊ10����, return 0������֡� */
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
/* ����������Ǵ���%20֮��Ķ�������"����"���̡�
 * %20 URL�����еġ� ��(space)
 * %21 '!' %22 '"' %23 '#' %24 '$'
 * %25 '%' %26 '&' %27 ''' %28 '('......
 * ������ܷ��������ļ����⡣ */
/**********************************************************************/
void strdecode(char *to, char *from)
{
    for ( ; *from != '\0'; ++to, ++from) {

        if (from[0] == '%' && isxdigit(from[1]) && isxdigit(from[2])) { //�����ж�from�� %20 �����ַ�

            *to = hexit(from[1])*16 + hexit(from[2]);//�ַ���E8�����������16���Ƶ�E8
            from += 2;                      //�ƹ��Ѿ�����������ַ�(%21ָ��ָ��1),���ʽ3��++from�����������һ���ַ�
        } else
            *to = *from;
    }
    *to = '\0';
}

/**********************************************************************/
/* ����fd�ϵ��¼�����������֮�󣬾���fd�ϵ�EPOLLONESHOT�¼�
 * ��ע�ᣬ���ǲ���ϵͳ��Ȼ�ᴥ��fd�ϵ�EPOLLIN�¼�����ֻ�ᴥ��һ�Ρ� */
/**********************************************************************/
void reset_oneshot( int epollfd, int fd )
{
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
    //event.events = EPOLLIN | EPOLLONESHOT;
    epoll_ctl( epollfd, EPOLL_CTL_MOD, fd, &event );
}
