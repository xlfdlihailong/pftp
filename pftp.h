#ifndef PFTP_H
#define PFTP_H
#include "Ftp.h"
#include "../plib/plib.h"
#define CMD_BUF_SIZE            256

int GetRetCodeWithRecv(int iSock, int iCorrectCode,char* arrchRecv);
int SendFtpCmdWithRecv(int iSock, const char *pchCmd, char *arrchRecv);
class pftp
{
    int sockStatus;//状态连接
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

    string pwd();
    //相对路径
    int cd(string strdes);
    int quit();
    //<0表示失败
    long long getLength(string path);
    int recvres(char* dataRecv,int len);

    int isExsistDir(string path);
    //被动模式，同时获取真实ip和端口，要在能访问的地方连接虚拟ip，否则不返回真实ip
    int setPASV();

    //默认自带断点续传，无限次数，直到传完才返回,支持服务端的断点续传
    //支持递归文件夹传输，且支持递归文件夹的各个文件的断点续传
    int upload(string strPathLocal, string strPathRemote);


    //默认自带断点续传，无限次数，直到传完才返回,仅支持服务端的断点续传
    //支持递归文件夹传输，且支持递归文件夹的各个文件的断点续传
    int download(string strPathLocal, string strPathRemote, long lpos=0);



    int UploadWithSpeedUpdateByXlfdInner(const char *pchLocalPath, const char *pchFtpPath, const char *pchPostfix , const int iPostfixFlag);
    int DownloadWithSpeedUpdateByXlfdInner(const char *pchLocalPath, const char *pchFtpPath, const char *pchPostfix , const int iPostfixFlag);
};

#endif // PFTP_H
