#ifndef PFTP_H
#define PFTP_H
#include "Ftp.h"
#include "../plib/plib.h"
#define CMD_BUF_SIZE            256
int UploadWithSpeed(const char *pchLocalPath, const char *pchFtpPath, const char *pchIP, int iCtrlSock, long long lPos, const char *pchPostfix , const int iPostfixFlag, char *arrchNowFile);
int DownloadWithSpeed(const char *pchLocalPath, const char *pchFtpPath, const char *pchIP, int iCtrlSock, long lPos, const char *pchPostfix);
class pftp
{
    ptcp *ptcpStatus;//状态连接
    ptcp* ptcpData;//数据连接

//    int iSockStatus;//状态连接
//    int iSockData;//数据连接

    string strIp;
    string strUser;
    string strPwd;
    int iPort;
public:
    //realip先设置为连接的ip，默认虚拟ip
    string strRealIp;
    //被动模式返回的端口
    int iPortPasv;
    pftp();
    ~pftp();
    int connect(string ip, string user, string pwd,int port=21);
    int reconnect();//一直重连，1s一次
    int sendcmd(string cmd);

    int quit();
    //<0表示失败
    long long getLength(string path);
    int recvres(char* dataRecv,int len);

    int setPASV();

    //默认自带断点续传，无限次数，直到传完才返回
    int upload(string strPathLocal, string strPathRemote);
    //lpos用于断点续传，默认是0 long 和 longlong在64位下都是8位范围是-9223372036854775807到9223372036854775807，足够
    int uploadNoReTrans(string strPathLocal, string strPathRemote, char *arrchNowFile,long long lpos=0);
    int download(string strPathLocal, string strPathRemote, long lpos=0);
};

#endif // PFTP_H
