/* ����Ǹ���С������ͷ�ļ������������������� */
#ifndef __FENGZHUANG_H_
#define __FENGZHUANG_H_
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <strings.h>

void perr_exit(const char *s);
int Accept(int fd, struct sockaddr *sa, socklen_t *salenptr);
int Bind(int fd, const struct sockaddr *sa, socklen_t salen);
int Listen(int fd, int backlog);
int Socket(int family, int type, int protocol);
ssize_t Read(int fd, void *ptr, size_t nbytes);
ssize_t Write(int fd, const void *ptr, size_t nbytes);
int Tcp3bind(short port,const char *IP);
void setnonblocking( int fd );
void addfd( int epollfd, int fd );
char *get_mime_type(char *name);
int hexit(char c);//16����ת10����
void strdecode(char *to, char *from);//����
void reset_oneshot( int epollfd, int fd );

#endif

