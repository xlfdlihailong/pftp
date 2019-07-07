/*
 * Usage:      Define FTP basic function.
 * Author:     SW
 * CreateTime：2013-05-31
 * modifier:   huanghd
 * modified time: 2014-04-02
 * modified desc: modify Upload,UploadConn
*/

#ifndef FTP_H_
#define FTP_H_
//suport c++
#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <dirent.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <sys/stat.h>
#include "Socket.h"
#include "../../c/clib/clib.h"

//PORT mode port
#define PORT_MIN_VAL 10000
#define PORT_MAX_VAL 20000
#define PORT_MODE 2
#define PASV_MODE 1

typedef struct
{
    char   arrchFileName[256];      //file or folder name
    char   arrchMonth[4];           //year or month
    char   arrchDay[4];             //day
    char   arrchHour[8];            //hour
    char   arrchUser[16];           //user name
    char   arrchGroup[16];          //group name
    char   arrchAttributes[12];     //property
    char   arrchSize[16];           //file or folder size
    char   arrchPreserved[8];       //preserved
}FTP_LIST_ITEM_STRUCT;

typedef struct
{
    char  arrchDate[20];        //日志记录时间
    int   iTransTime;           //传输时间，单位：s
    char  arrchSrcIP[16];       //数据源IP
    off_t oFileSize;            //传输字节数
    char  arrchFileName[256];   //传输文件名
    char  chTransMode;          //b表示二进制，a表示ASCII传输
    char  chBehavior;           //特殊动作标记，_表示没有动作，C/U/T表示文件是否被压缩
    char  chDescription;        //o表示下载，i表示上传
    char  chVisitMode;          //访问模式，r表示真是用户，a表示匿名，g表示Guest
    char  arrchLoginName[16];   //登录用户名称
    char  arrchFtpName[16];     //FTP服务器名称
    char  chAuthenticationMode; //0表示没有认证，1表示RFC931认证
    char  chFlag;               //表示如果认证，用户ID就无效
    char  chStatus;             //i表示本次文件未传完，c表示传输成功
}VSFTP_LOG_STRUCT;

//ftp struct
typedef struct
{
    char arrchIP[17];           //FTP地址
    int  iPort;                 //FTP端口
    char arrchUserName[128];    //FTP用户
    char arrchPassword[128];    //FTP密码
    char arrchPath[256];        //FTP路径
    long lPos;                  //文件偏移量
}FTP_STRUCT;

///add by lhl

///////////old
int GetRetCode(int iSock, int iCorrectCode);
int GetPASVRetCode(int iSock, int iCorrectCode,char *pchIP);
int SendFtpCmd(int iSock, const char *pchCmd);
int SendPASVFtpCmd(int iSock, const char *pchCmd,char *pchIP);
int ConnectFtp(int iSock, const char *pchIP, int iPort, const char *pchUser, const char *pchPwd);
int ParseFtpListCmd(int iCtrlSock, const char *pchFtpPath, const char *pchIP, FTP_LIST_ITEM_STRUCT **pstruFtpItem);
int Upload(const char *pchLocalPath, const char *pchFtpPath, const char *pchIP, int iCtrlSock, long lPos, const char *pchPostfix,const int iPostfixFlag);
int UploadConn(const char *pchLocalPath, const char *pchFtpPath, const char *pchIP,
               int iPort, const char *pchUser, const char *pchPwd, long lPos, const char *pchPostfix,const int iPostfixFlag);
int UploadBindSrcAddr(const char *pchLocalPath, const char *pchFtpPath, const char *pchIP, int iCtrlSock, long lPos, const char *pchPostfix ,const int iPostfixFlag, const char *pchSrcIP);
int UploadConnBindSrcAddr(const char *pchLocalPath, const char *pchFtpPath, const char *pchIP,
               int iPort, const char *pchUser, const char *pchPwd, long lPos, const char *pchPostfix,const int iPostfixFlag, const char *pchSrcIP);

int Download(const char *pchLocalPath, const char *pchFtpPath, const char *pchIP, int iCtrlSock, long lPos, const char *pchPostfix);
int DownloadConn(const char *pchLocalPath, const char *pchFtpPath, const char *pchIP,
                 int iPort, const char *pchUser, const char *pchPwd, long lPos, const char *pchPostfix);
int ParseVsftpLog(const char *pchLogPath, const char *pchFileName, VSFTP_LOG_STRUCT *pstruLog);

int Upload2(const char *pchLocalPath, const char *pchLocalIP,const char *pchFtpPath, const char *pchIP, int iCtrlSock, long lPos, const char *pchPostfix ,const int iPostfixFlag,const int iDataTransType);
int UploadConn2(const char *pchLocalPath,const char *pchLocalIP,const FTP_STRUCT *pstruRemoteFTP,const char *pchPostfix,const int iPostfixFlag,const int iDataTransType);
int Ip_Split(const char *String, char *delim, char Single[4][4]);
int PORTModeToServer(const int iCtrlSock,const char *pchLocalIP);
int GetServerIP(const char *pchOrgin,char *pchIP);
//add by panj 20151106, check file is exist or not, return 0:exist, -1: not exist
int CheckFileExist(const char *pchIP, int iPort, const char *pchUser, const char *pchPwd, const char *pRemoteFile);
int CheckFileExistBindSrcAddr(const char *pchIP, int iPort, const char *pchUser, const char *pchPwd, const char *pRemoteFile, const char *pchSrcIP);
int CheckDESCFileExistBindSrcAddr(const char *pchIP, int iPort, const char *pchUser, const char *pchPwd, const char *pRemoteFile, const char *pchSrcIP);
//support c++
#ifdef __cplusplus
}
#endif
#endif
