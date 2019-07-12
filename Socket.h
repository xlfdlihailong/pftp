/*
 * 作用：定义TCP套接字的基本操作。
 * 作者：SW
 * 创建日期：2010-08-10
 */

#ifndef TCPSOCK_H_
#define TCPSOCK_H_
//suport c++
#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <netinet/tcp.h>

#define MAX_LISTEN_NUM 5     /*请求连接的最大长度*/

int InitTcpSocket(void);
int InitUdpSocket(const char *pchIPAddr, int iPort, struct sockaddr_in *pstruAddr);
int InitSendGroupSocket(const char *pchIPAddr, const char *pchGroupIPAddr, int iGroupPort, struct sockaddr_in *pstruAddr);
int InitRecvGroupSocket(const char *pchIPAddr, const char *pchGroupIPAddr, int iGroupPort, struct sockaddr_in *pstruAddr);
int CreateConnection(int iSocket, const char *pchIPaddr, int iPort);
int BindSocket(int iSocket, const char *pchIPaddr, int iPort);
int BindUdpSocket(int iSocket, const char *pchIPAddr, int iPort, struct sockaddr_in *pstruAddr);
int AcceptConnection(int iSocket);
void CloseConnection(int iSocket);
int SendPacket(int iSocket, void *pBuffer, int uLength);
int RecvPacket(int iSocket, void *pBuffer, int uLength);
int SendFullPacket(int iSocket, void *pBuffer, int uLength);
int RecvFullPacket(int iSocket, void *pBuffer, int uLength);
int CheckNetwork(char *pchIpAddr);
int AutoRecvPacket(int iSocket, void *pBuffer, int iLength, char *pchIP, int iTimes);
int AutoSendPacket(int iSocket, void *pBuffer, int iLength, char *pchIP, int iTimes);
int KeepSockAlive(int iSock, int iAlvTm, int iIdleTm, int iIntTm, int iCnt);
//support c++
#ifdef __cplusplus
}
#endif
#endif

