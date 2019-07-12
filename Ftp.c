#include "Ftp.h"
#include "../../c/clib/clib.h"

#define CMD_BUF_SIZE            256
#define DEAL_RET_CODE(retCode)   { if ((retCode)) return retCode; }

/*
 * GetPort
 * pchOrgin:    PASV return value
 *
 * return:      port
 *
 * return port number of PASV command
*/
static int GetPort(const char *pchOrgin)
{
    int iShi;
    int iGe;
    char chPort[4];
    char *pchIndex;
    char *pchTemp;
    int i;

    memset(chPort, 0, sizeof(chPort));
    pchIndex = strstr(pchOrgin, "(");
    pchTemp = pchIndex + 1;
    for (i=0; i<4; i++)
    {
        pchIndex = strstr(pchTemp, ",");
        pchTemp = pchIndex + 1;
    }
    pchIndex = strstr(pchTemp, ",");
    memcpy(chPort, pchTemp, pchIndex - pchTemp);
    iShi = atoi(chPort);
    pchTemp = pchIndex + 1;

    pchIndex = strstr(pchTemp, ")");
    memset(chPort, 0, sizeof(chPort));
    memcpy(chPort, pchTemp, pchIndex - pchTemp);
    iGe = atoi(chPort);
    return iShi * 256 + iGe;
}

int GetPASVRetCode(int iSock, int iCorrectCode,char *pchIP)
{
    char arrchBuf[CMD_BUF_SIZE];
    char arrchCode[4];               /*FTP服务器端返回代码*/
    char *pchIndex;                 /*查找字符的位置*/
    int  iRet;
    int  iRecvSize = 0;
    struct timeval struTimeOut;

    //set connection timeout
    struTimeOut.tv_sec = 30;
    struTimeOut.tv_usec = 0;
    if (setsockopt(iSock, SOL_SOCKET, SO_RCVTIMEO, (char *)&struTimeOut, sizeof(struTimeOut)) == -1)
    {
        return -1;
    }

    //receive ftp server's return code
    do
    {
        iRet = RecvPacket(iSock, arrchBuf + iRecvSize, sizeof(arrchBuf));
        if (iRet < 0)
        {
            return -2;
        }
        else if (iRet == 0)
            break;
        iRecvSize += iRet;
        if(iRecvSize>CMD_BUF_SIZE)
        {
            HLOG("iRecvSize length >256,length = %d",iRet);
            return -2;
        }



    }
    while (arrchBuf[iRecvSize - 1] != '\n'); //use '\n' as end mark

    arrchBuf[iRecvSize] = 0;

    iRet = 0;
    pchIndex = strstr(arrchBuf, " ");
    //get code from result
    if (pchIndex != NULL)
    {
        memset(arrchCode, 0, sizeof(arrchCode));
        strncat(arrchCode, arrchBuf, pchIndex - arrchBuf);
        iRet = atoi(arrchCode);
    }

    HLOG_STRING(arrchBuf);
    //WriteLog(TRACE_DEBUG,"%s",arrchBuf);
    //suport PASV command
    if (iRet == 227)
    {
        GetServerIP(arrchBuf,pchIP);
        //        HLOG("##########pasv result: %s",arrchBuf);
        int iport= GetPort(arrchBuf);
//        HLOG("#############pasv return port: %d#################",iport);
        return iport;
    }
    //add by huanghd 20141027 support PORT mode
    if(iRet == 426)
    {
        return 0;
    }

    return iRet == iCorrectCode ? 0 : iRet;
}
/*
 * GetRetCode
 * iSock:        ctrl socket
 * iCorrectCode: correct ftp return code
 *
 * return:       success: 0
 *
 * Parse ftp server return code.
*/
int GetRetCode(int iSock, int iCorrectCode)
{
    char arrchBuf[CMD_BUF_SIZE];
    char arrchCode[4];               /*FTP服务器端返回代码*/
    char *pchIndex;                 /*查找字符的位置*/
    int  iRet;
    int  iRecvSize = 0;
    struct timeval struTimeOut;

    //set connection timeout
    struTimeOut.tv_sec = 5 ;
    struTimeOut.tv_usec = 0;
    if (setsockopt(iSock, SOL_SOCKET, SO_RCVTIMEO, (char *)&struTimeOut, sizeof(struTimeOut)) == -1)
    {
        HLOG("setopt fail");
        return -1;
    }

    //接收服务端返回的数据，直到遇到\n作为结束标志
    do
    {
        //不知道收多大，直接收256
        iRet = RecvPacket(iSock, arrchBuf + iRecvSize, sizeof(arrchBuf));
        if (iRet < 0)
        {
            HLOG("RecvPacket error , %d", iRet);
            return -2;
        }
        else if (iRet == 0)
            break;
        iRecvSize += iRet;
        if(iRecvSize>=CMD_BUF_SIZE)
        {
            HLOG("iRecvSize = %d >MAX_LEN 256, recv fail!", iRecvSize);
            return -2;
        }
    }
    while (arrchBuf[iRecvSize - 1] != '\n'); //use '\n' as end mark

    arrchBuf[iRecvSize] = 0;

    //WriteLog(TRACE_DEBUG,"%s",arrchBuf);

    iRet = 0;
    pchIndex = strstr(arrchBuf, " ");
    //get code from result
    if (pchIndex != NULL)
    {
        memset(arrchCode, 0, sizeof(arrchCode));
        strncat(arrchCode, arrchBuf, pchIndex - arrchBuf);
        iRet = atoi(arrchCode);
    }

//    if(iRet==425||iRet==550)
//        HLOG("ftp服务返回字符串：%s",arrchBuf);


    //suport PASV command
    if (iRet == 227)
    {
        return GetPort(arrchBuf);
    }
    //add by huanghd 20141027 support PORT mode
    if (iRet == 426)
    {
        return 0;
    }

    return iRet == iCorrectCode ? 0 : iRet;
}

/*
 * IsDirExists
 * pchPath：路径
 * pchDir： 目录
 * 返回值：     大于0表示存在
 *              0表示路径存在，目录不存在
 *              -1表示路径不存在
 *
 * 判断目录是否存在。
 */
static int IsDirExists(char *pchPath, char *pchDir)
{
    DIR *dp;
    struct dirent *dirp;
    struct stat statbuf;
    int iRet = 0;

    if ((dp = opendir(pchPath)) == NULL)
    {
        return -1;
    }
    while ((dirp = readdir(dp)) != NULL)
    {
        lstat(dirp->d_name, &statbuf);
        if (!strcmp(pchDir, dirp->d_name) && !S_ISDIR(statbuf.st_mode))
        {
            iRet = 1;
            break;
        }
    }
    closedir(dp);
    return iRet;
}

/*
 * BuildDirectory
 * pchFile：目录路径字符串，例如/tmp/20100301/
 * 返回值： 0成功
 *
 * 递归创建目录
*/
static int BuildDirectory(const char *pchPath)
{
    char *pch = NULL;
    char chBuf[512 + 1] = {0};

    //解析路径中的文件夹和文件，并创建
    for (pch = (char *)pchPath; abs((pch = strchr(pch, '/')) - pchPath) < strlen(pchPath); pch++)
    {
        memset(chBuf, 0, sizeof(chBuf));
        memcpy(chBuf, pchPath, pch - pchPath);
        mkdir(chBuf, S_IRWXU | S_IRWXG | S_IRWXO);
    }

    return 0;
}

/*
 * ConnectFtp
 * iSock:        tcp socket
 * pchIP:        IP address
 * iPort:        ftp port
 * pchUser:      ftp user
 * pchPwd:       ftp password
 *
 * return:       success: 0
 *
 * Connect ftp server with user name and password.
 *
*/
int ConnectFtp(int iSock, const char *pchIP, int iPort, const char *pchUser, const char *pchPwd)
{
    char arrchBuf[CMD_BUF_SIZE];     /*指令缓冲区*/
    int iRet;                        /*执行结果*/

    HLOG("正在连接ftp服务端：%s",pchIP);
    //建立和FTP服务器的控制连接，如果连接建立失败，返回错误代码
    if (CreateConnection(iSock, pchIP, iPort) != 0)
    {
        HLOG("连接ftp服务端：%s失败",pchIP);
        return -1;
    }
    //接收FTP服务器的返回代码，如果接收失败，返回错误代码
    iRet = GetRetCode(iSock, 220);
//    HLOG("GetRetCode:%d",iRet);
    DEAL_RET_CODE(iRet);


    //向FTP服务器发送用户名
    memset(arrchBuf, 0, sizeof(arrchBuf));
    if (pchUser == NULL || !strcmp(pchUser, ""))
        sprintf(arrchBuf, "USER anonymous\r\n");
    else
        sprintf(arrchBuf, "USER %s\r\n", pchUser);
//    HLOG("SendPacket %s",pchUser);
    if (0 >= SendPacket(iSock, arrchBuf, strlen(arrchBuf)))
    {
        return -2;
    }
    //接收FTP服务器的返回代码，如果接收失败，返回错误代码
    iRet = GetRetCode(iSock, 331);
//    HLOG("GetRetCode:%d",iRet);

    //如果FTP服务器返回代码331， 说明需要密码
    if (iRet < 0)
    {
        return -3;
    }
    else if (iRet == 230)
    {
        return 0;
    }

    //向FTP服务器发送密码
    memset(arrchBuf, 0, sizeof(arrchBuf));
    sprintf(arrchBuf, "PASS %s\r\n", pchPwd);
//    HLOG("SendPacket %s",pchPwd);
    if (0 >= SendPacket(iSock, arrchBuf, strlen(arrchBuf)))
    {
        return -4;
    }

    //接收FTP服务器的返回代码，如果接收失败，返回错误代码
    iRet = GetRetCode(iSock, 230);
//    HLOG("GetRetCode:%d",iRet);
    DEAL_RET_CODE(iRet);

    return 0;
}

/*
 * GetCorrectCode
 * pchCmd:       ftp command
 *
 * return:       correct code
 *
 * Get ftp correct code of specific command.
 *
*/
static int GetCorrectCode(const char *pchCmd)
{
    if (pchCmd == NULL)
        return -1;

    if (!strncmp(pchCmd, "QUIT", 4))
        return 221;
    else if (!strncmp(pchCmd, "TYPE", 4))
        return 200;
    else if (!strncmp(pchCmd, "DELE", 4))
        return 250;
    else if (!strncmp(pchCmd, "REST", 4))
        return 350;
    else if (!strncmp(pchCmd, "LIST", 4))
        return 150;
    else if (!strncmp(pchCmd, "RETR", 4))
        return 150;
    else if (!strncmp(pchCmd, "PASV", 4))
        return 227;
    else if (!strncmp(pchCmd, "STOR", 4))
        return 150;
    else if (!strncmp(pchCmd, "CDUP", 4))
        return 200;
    else if (!strncmp(pchCmd, "RETR", 4))
        return 150;
    else if (!strncmp(pchCmd, "APPE", 4))
        return 150;
    else if (!strncmp(pchCmd, "RNFR", 4))
        return 350;
    else if (!strncmp(pchCmd, "RNTO", 4))
        return 250;
    else if (!strncmp(pchCmd, "CWD", 3))
        return 250;
    else if (!strncmp(pchCmd, "PWD", 3))
        return 257;
    else if (!strncmp(pchCmd, "MKD", 3))
        return 257;
    else if (!strncmp(pchCmd, "RMD", 3))
        return 250;
    else if(!strncmp(pchCmd, "PORT", 4))
        return 200;
    else if(!strncmp(pchCmd,"SIZE",4))//add by xlfd//如果返回码为213，则任务成功返回远程服务器文件大小
        return 213;

    return -2;
}

/*
 * SendFtpCmd
 * iSock ：FTP连接套接字
 * pchCmd：FTP命令
 * 返回值：FTP返回的代码，失败返回值小于0
 *         PASV 返回的是分配的端口号
 *
 * 向FTP服务器发送命令：
 * TYPE I：转换传输类型
 * PASV  ：被动模式
 * PORT  :主动模式
 * DELE XX：删除文件
 * STOR XX：上传文件
 * REST 1234：指定FTP服务器上文件的写入位置
 * MKD XX：创建文件夹
 * CWD XX：改变当前路径
*/
int SendFtpCmd(int iSock, const char *pchCmd)
{
    char arrchBuf[CMD_BUF_SIZE];     /*指令缓冲区*/
    int iRet;                        /*执行结果*/
    struct timeval struTimeOut;

    //填写超时时间
    struTimeOut.tv_sec = 5;
    struTimeOut.tv_usec = 0;

    //设置超时时间
    if (setsockopt(iSock, SOL_SOCKET, SO_SNDTIMEO, (char *)&struTimeOut, sizeof(struTimeOut)) == -1)
    {
        return -1;
    }

    memset(arrchBuf, 0, sizeof(arrchBuf));
    sprintf(arrchBuf, "%s\r\n", pchCmd);

#ifdef DEBUG
    HLOG( "%s", arrchBuf);
#endif

    if ((iRet = GetCorrectCode(pchCmd)) < 0)
    {
        HLOG("pchCmd:%s",pchCmd);
        return -2;
    }
    if (0 >= SendPacket(iSock, arrchBuf, strlen(arrchBuf)))
    {
        return -3;
    }
    //接收FTP服务器的返回代码，如果接收失败，返回错误代码
    //HLOG( "%s", pchCmd);
    iRet = GetRetCode(iSock, iRet);
    if (!strncmp(pchCmd, "PASV", 4))
    {
        HLOG( "%s", pchCmd);
        return iRet;
    }
    //HLOG( "%s", pchCmd);
    //    HLOG("%d",iRet);
    DEAL_RET_CODE(iRet);

    return 0;
}

/*
 * SendFtpCmd
 * iSock ：FTP连接套接字
 * pchCmd：FTP命令
 * 返回值：FTP返回的代码，失败返回值小于0
 *         PASV 返回的是分配的端口号
 *
 * 向FTP服务器发送命令：
 * PASV  ：被动模式
*/
int SendPASVFtpCmd(int iSock, const char *pchCmd,char *pchIP)
{
    char arrchBuf[CMD_BUF_SIZE];     /*指令缓冲区*/
    int iRet;                        /*执行结果*/
    struct timeval struTimeOut;

    //填写超时时间
    struTimeOut.tv_sec = 30;
    struTimeOut.tv_usec = 0;

    //设置超时时间
    if (setsockopt(iSock, SOL_SOCKET, SO_SNDTIMEO, (char *)&struTimeOut, sizeof(struTimeOut)) == -1)
    {
        return -1;
    }

    memset(arrchBuf, 0, sizeof(arrchBuf));
    sprintf(arrchBuf, "%s\r\n", pchCmd);


    //    HLOG( "%s", arrchBuf);

    if ((iRet = GetCorrectCode(pchCmd)) < 0)
    {
        return -2;
    }
    if (0 >= SendPacket(iSock, arrchBuf, strlen(arrchBuf)))
    {
        return -3;
    }
    //接收FTP服务器的返回代码，如果接收失败，返回错误代码
    //    HLOG( "%s", pchCmd);
    iRet = GetPASVRetCode(iSock,iRet,pchIP);
    if (!strncmp(pchCmd, "PASV", 4))
    {
        return iRet;
    }
    DEAL_RET_CODE(iRet);

    return 0;
}
/*
 * ParseFtpListCmd
 * iCtrlSock:   控制套接字，如果不为0，则说明用此套接字
 * pchFtpPath： ftp路径，后面包含'/'，例如sw/trans/core/
 * pchIP：      IP地址
 * pstruFtpItem:FTP文件结构
 * 返回值：  成功返回文件总数（包括文件夹个数）， 失败返回值小于0
 *
 * 向FTP发送LIST命令，并解析。
*/
int ParseFtpListCmd(int iCtrlSock, const char *pchFtpPath, const char *pchIP, FTP_LIST_ITEM_STRUCT **pstruFtpItem)
{
    char   *pchOuter;                               /*临时指针*/
    char   *pchTok;                                 /*临时指针*/
    char   arrchBuf[CMD_BUF_SIZE * CMD_BUF_SIZE];   /*指令缓冲区*/
    char   *pchBuf;                                 /*临时指针*/
    int    iItemNum = 0;                            /*文件数量*/
    int    i;                                       /*循环变量i*/
    int    iRecvBytes;                              /*接收字节数*/
    int    iSendSock;                               /*数据socket*/
    int    iRet;                                    /*执行结果*/
    fd_set fdRead;                                  /*可读*/
    struct timeval struTimeOut;                     /*超时*/

    FD_ZERO(&fdRead);
    FD_SET(iCtrlSock, &fdRead);
    iSendSock = InitTcpSocket();
    struTimeOut.tv_sec = 1;

    //使用被动模式建立和FTP服务器的连接
    if ((iRet = SendFtpCmd(iCtrlSock, "PASV")) < 0)
    {
        close(iSendSock);
        HLOG("发送PASV命令失败:%s",strerror(errno));
        return -1;
    }
    //建立TCP连接
    if (CreateConnection(iSendSock, pchIP, iRet) < 0)
    {
        close(iSendSock);
        HLOG("建立TCP连接失败:%s",strerror(errno));
        return -2;
    }

    //向FTP服务器21端口发送ls命令
    memset(arrchBuf, 0, sizeof(arrchBuf));
    sprintf(arrchBuf, "LIST %s", pchFtpPath);
    if ((iRet = SendFtpCmd(iCtrlSock, arrchBuf)))
    {
        close(iSendSock);
        return -3;
    }

    iRet = iRecvBytes = 0;
    //如果返回缓冲区不为空，则从中获取任务文件名
    while (1)
    {
        if ((iRet = RecvPacket(iSendSock, arrchBuf + iRet, sizeof(arrchBuf))) <= 0)
        {
            break;
        }
        iRecvBytes += iRet;
    }

    //get ftp server return code
    if ((iRet = GetRetCode(iCtrlSock, 226)))
    {
        close(iSendSock);
        return iRet;
    }
    pchBuf = arrchBuf;
    //WriteLog(TRACE_DEBUG, "%s", arrchBuf);

    //get item number
    while (*pchBuf != '\0')
    {
        if (*pchBuf == '\r' && *(pchBuf + 1) == '\n')
            iItemNum++;
        pchBuf++;
    }

    if ((*pstruFtpItem = (FTP_LIST_ITEM_STRUCT *)malloc(sizeof(FTP_LIST_ITEM_STRUCT) * iItemNum)) == NULL)
    {
        close(iSendSock);
        return -4;
    }
    iItemNum = 0;
    pchBuf = arrchBuf;

    //解析服务器返回的字符串
    while (iRecvBytes > 0 && (pchTok = strtok_r(pchBuf, "\r\n", &pchOuter)) != NULL)
    {
        //Serv-U will return 'total x'
        if (!strncmp(pchTok, "total", 5))
        {
            pchBuf = NULL;
            continue;
        }
        pchBuf = pchTok;
        i = 0;

        memset(&(*pstruFtpItem)[iItemNum], 0, sizeof(FTP_LIST_ITEM_STRUCT));
        //逐个字符进行解析，以空格为分隔符
        while (pchBuf != '\0')
        {
            while (*pchBuf == ' ')
            {
                pchBuf++;
            }

            pchTok = strchr(pchBuf, ' ');  //找到下个空格的地方
            //如果是文件名，则忽略掉中间的空格，因为文件名可以包含空格（不同FTP服务器的实现可能不一样）
            if (i == 8)
            {
                pchTok = '\0';
            }

            switch (i)
            {
            case 0: //属性
                memcpy((*pstruFtpItem)[iItemNum].arrchAttributes, pchBuf, pchTok - pchBuf); break;
            case 1: //预留
                memcpy((*pstruFtpItem)[iItemNum].arrchPreserved, pchBuf, pchTok - pchBuf); break;
            case 2: //用户
                memcpy((*pstruFtpItem)[iItemNum].arrchUser, pchBuf, pchTok - pchBuf); break;
            case 3: //组
                memcpy((*pstruFtpItem)[iItemNum].arrchGroup, pchBuf, pchTok - pchBuf); break;
            case 4: //大小
                memcpy((*pstruFtpItem)[iItemNum].arrchSize, pchBuf, pchTok - pchBuf); break;
            case 5: //月
                memcpy((*pstruFtpItem)[iItemNum].arrchMonth, pchBuf, pchTok - pchBuf); break;
            case 6: //日
                memcpy((*pstruFtpItem)[iItemNum].arrchDay, pchBuf, pchTok - pchBuf); break;
            case 7: //年或时间
                memcpy((*pstruFtpItem)[iItemNum].arrchHour, pchBuf, pchTok - pchBuf); break;
            case 8: //文件名
                strcpy((*pstruFtpItem)[iItemNum].arrchFileName, pchBuf); break;
            default: break;
            }
            pchBuf = pchTok;
            i++;
        }

        pchBuf = NULL;
        if (strcmp(".", (*pstruFtpItem)[iItemNum].arrchFileName)
                && strcmp("..", (*pstruFtpItem)[iItemNum].arrchFileName)
                && strcmp("", (*pstruFtpItem)[iItemNum].arrchFileName))
            iItemNum++;
    }

    close(iSendSock);
    return iItemNum;
}

/*
 * Upload
 * pchLocalPath: local file path, supoort like "xx", "./xx", "xx/"
 * pchFtpPath:   ftp dir path, support like "", "./", ".", "xx", "xx/"
 * pchIP:        IP address
 * iCtrlSock:    ctrl socket
 * lPos:         file position
 * pchPostfix:   transport file whether using postfix
 * iPostfixFlag  postfix flag (0 replace 1 postfix)
 * return:       success: 0
 *
 * Upload a file or folder to the ftp server's specific path.
 *
*/
int Upload(const char *pchLocalPath, const char *pchFtpPath, const char *pchIP, int iCtrlSock, long lPos, const char *pchPostfix ,const int iPostfixFlag)
{
    char arrchBuf[CMD_BUF_SIZE * 16];  //缓冲区
    char arrchFtpPath[CMD_BUF_SIZE];   //FTP路径
    char arrchLocalPath[CMD_BUF_SIZE]; //本地路径
    char arrchTmp[CMD_BUF_SIZE];       //临时缓冲区
    char *pchTok;                      //临时指针
    char *pchFtpFileName;              //FTP文件名
    char *pchOuter;                    //指针
    char *pchStr;                      //字符指针
    int iRet;                          //返回值
    long long llRet;                          //返回值
    int iDataSock;                     //数据套接字
    int iLevel = 0;                    //FTP级别
    int i;                             //循环变量
    long long llFileSize;                    //文件大小
    long long llSentSize = 0;                //发送大小
    FILE *fp;                          //文件指针
    DIR                  *pDp;         //目录指针
    struct dirent        *pstruDir;    //目录结构指针
    struct stat          struStat;     //文件状态指针



    //validate input paras, only support upload dir and regular file
    stat(pchLocalPath, &struStat);
    if (pchFtpPath == NULL || pchIP == NULL || lPos < 0 || iCtrlSock < 2 ||
            (!S_ISDIR(struStat.st_mode) && !S_ISREG(struStat.st_mode)))
    {
        return -1;
    }

    //standard ftp remote dir path
    iRet = strlen(pchFtpPath);
    memcpy(arrchFtpPath, pchFtpPath, iRet);
    arrchFtpPath[iRet] = 0;
    if (arrchFtpPath[iRet - 1] == '/')
        arrchFtpPath[iRet - 1] = 0;

    //standard local path
    iRet = strlen(pchLocalPath);
    memcpy(arrchLocalPath, pchLocalPath, iRet);
    arrchLocalPath[iRet] = 0;
    if (arrchLocalPath[iRet - 1] == '/')
        arrchLocalPath[iRet - 1] = 0;

    //if local path is dir, then upload dir
    if (S_ISDIR(struStat.st_mode))
    {
        //open local dir
        if ((pDp = opendir(pchLocalPath)) == NULL)
        {
            return -2;
        }

        //create ftp dir path
        pchTok = strrchr(arrchLocalPath, '/');
        strcat(arrchFtpPath, "/");
        strcat(arrchFtpPath, pchTok != NULL ? pchTok + 1 : arrchLocalPath);

        //read local dir information and upload one by one
        while ((pstruDir = readdir(pDp)) != NULL)
        {
            //ignore folder . and ..
            if (!strcmp(pstruDir->d_name, ".") || !strcmp(pstruDir->d_name, ".."))
            {
                continue;
            }
            memset(arrchBuf, 0, sizeof(arrchBuf));
            sprintf(arrchBuf, "%s/%s", arrchLocalPath, pstruDir->d_name);

            //upload file or dir
            if ((iRet = Upload(arrchBuf, arrchFtpPath, pchIP, iCtrlSock, 0, pchPostfix,iPostfixFlag)))
            {
                closedir(pDp);
                return iRet;
            }
        }
        closedir(pDp);
        return 0;
    }

    //upload regular file
    if (strcmp(arrchFtpPath, "") && strcmp(arrchFtpPath, "."))
    {
        strcat(arrchFtpPath, "/");
    }

    //deal ftp path like "", "." or "dir_name"
    pchTok = strrchr(arrchLocalPath, '/');
    strcat(arrchFtpPath, pchTok != NULL ? pchTok + 1 : arrchLocalPath);

    //get ftp dir path
    memset(arrchBuf, 0, sizeof(arrchBuf));
    pchFtpFileName = strrchr(arrchFtpPath, '/');
    pchFtpFileName = pchFtpFileName == NULL ? arrchFtpPath : pchFtpFileName + 1;
    memcpy(arrchBuf, arrchFtpPath, pchFtpFileName - arrchFtpPath);
    pchStr = arrchBuf;
    //if ftp dir doesn't exist, then create it
    while ((pchTok = strtok_r(pchStr, "/", &pchOuter)) != NULL)
    {
        //ignore path name .
        if (pchTok[0] == '.')
        {
            pchStr = NULL;
            continue;
        }
        //try enter ftp dir
        sprintf(arrchTmp, "CWD %s", pchTok);
        if (SendFtpCmd(iCtrlSock, arrchTmp))
        {
            //create ftp dir
            sprintf(arrchTmp, "MKD %s", pchTok);
            if ((iRet = SendFtpCmd(iCtrlSock, arrchTmp)))
            {
                return iRet;
            }

            //enter ftp dir
            sprintf(arrchTmp, "CWD %s", pchTok);
            if ((iRet = SendFtpCmd(iCtrlSock, arrchTmp)))
            {
                return iRet;
            }
        }
        pchStr = NULL;
        iLevel++; //ftp dir path depth
    }

    //connect ftp server using PASV mode
    memset(arrchTmp,0,sizeof(arrchTmp));
    if ((iRet = SendPASVFtpCmd(iCtrlSock, "PASV",arrchTmp)) < 0)
    {
        return iRet;
    }

    //it`s up to callers to use PASV or PORT mode. by hhd 20141014
    //connect ftp server using PASV mode
    //    if ((iRet = SendFtpCmd(iCtrlSock, "PORT")) < 0)
    //    {
    //        return iRet;
    //    }

    //create data socket connection.add judgement by hhd 20131122
    if((iDataSock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        HLOG("创建数据连接失败，失败原因：%s",strerror(errno));
        return -2;
    }

    if (0 > CreateConnection(iDataSock, arrchTmp, iRet))
    {
        HLOG("CreateConnection error:%d, %s",errno,strerror(errno));
        close(iDataSock);
        return -3;
    }

    //change transfer mode to binary
    if ((iRet = SendFtpCmd(iCtrlSock, "TYPE I")))
    {
        HLOG("TYPE I error:%d, %s",errno,strerror(errno));
        close(iDataSock);
        return iRet;
    }
    HLOG("TYPE I");

    //open local file and get its size
    if ((fp = fopen(pchLocalPath, "r")) == NULL)
    {
        close(iDataSock);
        return -4;
    }


    //fseek to the end of the file
    fseek(fp, 0L, SEEK_END);
    llFileSize = ftell(fp);
    llFileSize -= lPos;
    fseek(fp, lPos, SEEK_SET);

    //send STOR command to ftp server
    if (!lPos)
        sprintf(arrchBuf, "STOR %s", pchFtpFileName);
    else
        sprintf(arrchBuf, "APPE %s", pchFtpFileName);

    //postfix upload if exist
    if (pchPostfix != NULL && strlen(pchPostfix) > 0)
    {
        //replace postfix
        if(iPostfixFlag == 0)
        {
            memset(arrchTmp,0,sizeof(arrchTmp));
            pchStr = strrchr(arrchBuf,'.');
            memcpy(arrchTmp,arrchBuf,pchStr - arrchBuf);
            memset(arrchBuf,0,sizeof(arrchBuf));
            memcpy(arrchBuf,arrchTmp,strlen(arrchTmp));
        }
        strcat(arrchBuf, pchPostfix);
    }

    if ((iRet = SendFtpCmd(iCtrlSock, arrchBuf)))
    {
        HLOG("%s error:%d, %s, iRet:%d",arrchBuf,errno,strerror(errno),iRet);
        close(iDataSock);
        fclose(fp);
        return iRet;
    }
    HLOG("%s",arrchBuf);

    //send local file
    while (llSentSize < llFileSize)
    {
        if ((llRet = llFileSize - llSentSize) > CMD_BUF_SIZE * 16)
        {
            llRet = CMD_BUF_SIZE * 16;
        }
        //read file and upload
        llRet = fread(arrchBuf, sizeof(char), llRet, fp);
        if ((llRet = SendFullPacket(iDataSock, arrchBuf, llRet)) <= 0)
        {
            break;
        }
        llSentSize += llRet;
    }
    fclose(fp);
    close(iDataSock);

    HLOG("Finish");
    //get ftp server return code
    if ((iRet = GetRetCode(iCtrlSock, 226)))
    {
        return iRet;
    }

    //if postfix, rename file name
    if (pchPostfix != NULL && strlen(pchPostfix) > 0)
    {
        memset(arrchBuf, 0, sizeof(arrchBuf));
        //replace postfix
        if(iPostfixFlag == 0)
        {
            memset(arrchTmp,0,sizeof(arrchTmp));
            pchStr = strrchr(pchFtpFileName,'.');
            memcpy(arrchTmp,pchFtpFileName,pchStr - pchFtpFileName);
        }
        else
        {
            strcpy(arrchTmp,pchFtpFileName);
        }
        strcat(arrchTmp,pchPostfix);
        sprintf(arrchBuf, "RNFR %s", arrchTmp);
        if ((iRet = SendFtpCmd(iCtrlSock, arrchBuf)))
        {
            return iRet;
        }

        memset(arrchBuf, 0, sizeof(arrchBuf));
        sprintf(arrchBuf, "RNTO %s", pchFtpFileName);
        if ((iRet = SendFtpCmd(iCtrlSock, arrchBuf)))
        {
            return iRet;
        }
    }

    //change to previous ftp directory
    for (i=0; i<iLevel; i++)
    {
        iRet = SendFtpCmd(iCtrlSock, "CDUP");
    }

    //send failed if filesize greater than zero
    if (llSentSize < llFileSize)
    {
        return -5;
    }

    return 0;
}

/*
 * Upload
 * pchLocalPath: local file path, supoort like "xx", "./xx", "xx/"
 * pchFtpPath:   ftp dir path, support like "", "./", ".", "xx", "xx/"
 * pchIP:        IP address
 * iCtrlSock:    ctrl socket
 * lPos:         file position
 * pchPostfix:   transport file whether using postfix
 * iPostfixFlag  postfix flag (0 replace 1 postfix)
 * return:       success: 0
 *
 * Upload a file or folder to the ftp server's specific path.
 *
*/
int UploadBindSrcAddr(const char *pchLocalPath, const char *pchFtpPath, const char *pchIP, int iCtrlSock, long lPos, const char *pchPostfix ,const int iPostfixFlag, const char *pchSrcIP)
{
    char arrchBuf[CMD_BUF_SIZE * 16];  //缓冲区
    char arrchFtpPath[CMD_BUF_SIZE];   //FTP路径
    char arrchLocalPath[CMD_BUF_SIZE]; //本地路径
    char arrchTmp[CMD_BUF_SIZE];       //临时缓冲区
    char *pchTok;                      //临时指针
    char *pchFtpFileName;              //FTP文件名
    char *pchOuter;                    //指针
    char *pchStr;                      //字符指针
    int iRet;                          //返回值
    long long llRet;                          //返回值
    int iDataSock;                     //数据套接字
    int iLevel = 0;                    //FTP级别
    int i;                             //循环变量
    long long llFileSize;                    //文件大小
    long long llSentSize = 0;                //发送大小
    FILE *fp;                          //文件指针
    DIR                  *pDp;         //目录指针
    struct dirent        *pstruDir;    //目录结构指针
    struct stat          struStat;     //文件状态指针



    //validate input paras, only support upload dir and regular file
    stat(pchLocalPath, &struStat);
    if (pchFtpPath == NULL || pchIP == NULL || lPos < 0 || iCtrlSock < 2 ||
            (!S_ISDIR(struStat.st_mode) && !S_ISREG(struStat.st_mode)))
    {
        HLOG("本地路径%s不存在",pchLocalPath);

        return -1;
    }

    //standard ftp remote dir path
    iRet = strlen(pchFtpPath);
    memcpy(arrchFtpPath, pchFtpPath, iRet);
    arrchFtpPath[iRet] = 0;
    if (arrchFtpPath[iRet - 1] == '/')
        arrchFtpPath[iRet - 1] = 0;

    //standard local path
    iRet = strlen(pchLocalPath);
    memcpy(arrchLocalPath, pchLocalPath, iRet);
    arrchLocalPath[iRet] = 0;
    if (arrchLocalPath[iRet - 1] == '/')
        arrchLocalPath[iRet - 1] = 0;

    //if local path is dir, then upload dir
    //这是递归上传目录
    if (S_ISDIR(struStat.st_mode))
    {
        //open local dir
        if ((pDp = opendir(pchLocalPath)) == NULL)
        {
            return -2;
        }

        //create ftp dir path
        pchTok = strrchr(arrchLocalPath, '/');
        strcat(arrchFtpPath, "/");
        strcat(arrchFtpPath, pchTok != NULL ? pchTok + 1 : arrchLocalPath);

        //read local dir information and upload one by one
        while ((pstruDir = readdir(pDp)) != NULL)
        {
            //ignore folder . and ..
            if (!strcmp(pstruDir->d_name, ".") || !strcmp(pstruDir->d_name, ".."))
            {
                continue;
            }
            memset(arrchBuf, 0, sizeof(arrchBuf));
            sprintf(arrchBuf, "%s/%s", arrchLocalPath, pstruDir->d_name);

            //upload file or dir
            if ((iRet = Upload(arrchBuf, arrchFtpPath, pchIP, iCtrlSock, 0, pchPostfix,iPostfixFlag)))
            {
                closedir(pDp);
                return iRet;
            }
        }
        closedir(pDp);
        return 0;
    }

    //upload regular file
    if (strcmp(arrchFtpPath, "") && strcmp(arrchFtpPath, "."))
    {
        strcat(arrchFtpPath, "/");
    }

    //deal ftp path like "", "." or "dir_name"
    pchTok = strrchr(arrchLocalPath, '/');
    strcat(arrchFtpPath, pchTok != NULL ? pchTok + 1 : arrchLocalPath);

    //get ftp dir path
    memset(arrchBuf, 0, sizeof(arrchBuf));
    pchFtpFileName = strrchr(arrchFtpPath, '/');
    pchFtpFileName = pchFtpFileName == NULL ? arrchFtpPath : pchFtpFileName + 1;
    memcpy(arrchBuf, arrchFtpPath, pchFtpFileName - arrchFtpPath);
    pchStr = arrchBuf;
    //if ftp dir doesn't exist, then create it
    while ((pchTok = strtok_r(pchStr, "/", &pchOuter)) != NULL)
    {
        //ignore path name .
        if (pchTok[0] == '.')
        {
            pchStr = NULL;
            continue;
        }
        //try enter ftp dir
        sprintf(arrchTmp, "CWD %s", pchTok);
        if (SendFtpCmd(iCtrlSock, arrchTmp))
        {
            //create ftp dir
            sprintf(arrchTmp, "MKD %s", pchTok);
            if ((iRet = SendFtpCmd(iCtrlSock, arrchTmp)))
            {
                return iRet;
            }

            //enter ftp dir
            sprintf(arrchTmp, "CWD %s", pchTok);
            if ((iRet = SendFtpCmd(iCtrlSock, arrchTmp)))
            {
                return iRet;
            }
        }
        pchStr = NULL;
        iLevel++; //ftp dir path depth
    }

    //connect ftp server using PASV mode
    memset(arrchTmp,0,sizeof(arrchTmp));
    //strcpy(arrchTmp,pchIP);
    //这个返回的iRet改为端口号了
    if ((iRet = SendPASVFtpCmd(iCtrlSock, "PASV",arrchTmp)) < 0)
    {
        return iRet;
    }
    HLOG("pasv port: %d",iRet);

    //it`s up to callers to use PASV or PORT mode. by hhd 20141014
    //connect ftp server using PASV mode
    //    if ((iRet = SendFtpCmd(iCtrlSock, "PORT")) < 0)
    //    {
    //        return iRet;
    //    }

    //create data socket connection.add judgement by hhd 20131122
    if((iDataSock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        HLOG("创建数据连接失败，失败原因：%s",strerror(errno));
        return -2;
    }

    //bind src addr,add by panj 20160117
    if(pchSrcIP != NULL)
    {
        struct sockaddr_in sin;
        sin.sin_family = AF_INET;
        sin.sin_addr.s_addr = inet_addr(pchSrcIP);
        sin.sin_port = INADDR_ANY;
        //        HLOG("pasv port:%d",sin.sin_port);
        bind(iDataSock,(struct sockaddr *)&sin, sizeof(struct sockaddr_in));
    }
    //向指定ip和端口建立链接
    if (0 > CreateConnection(iDataSock, arrchTmp, iRet))
    {
        close(iDataSock);
        return -3;
    }

    //change transfer mode to binary
    if ((iRet = SendFtpCmd(iCtrlSock, "TYPE I")))
    {
        close(iDataSock);
        return iRet;
    }

    //open local file and get its size
    if ((fp = fopen(pchLocalPath, "r")) == NULL)
    {
        close(iDataSock);
        return -4;
    }

    //fseek to the end of the file
    fseek(fp, 0L, SEEK_END);
    llFileSize = ftell(fp);
    llFileSize -= lPos;
    fseek(fp, lPos, SEEK_SET);

    //send STOR command to ftp server
    if (!lPos)
        sprintf(arrchBuf, "STOR %s", pchFtpFileName);
    else
        sprintf(arrchBuf, "APPE %s", pchFtpFileName);
    HLOG("preCMDTOFTP: %s",arrchBuf);
    //postfix upload if exist
    if (pchPostfix != NULL && strlen(pchPostfix) > 0)
    {
        //replace postfix
        if(iPostfixFlag == 0)
        {
            memset(arrchTmp,0,sizeof(arrchTmp));
            pchStr = strrchr(arrchBuf,'.');
            memcpy(arrchTmp,arrchBuf,pchStr - arrchBuf);
            memset(arrchBuf,0,sizeof(arrchBuf));
            memcpy(arrchBuf,arrchTmp,strlen(arrchTmp));
        }
        strcat(arrchBuf, pchPostfix);
    }

    if ((iRet = SendFtpCmd(iCtrlSock, arrchBuf)))
    {
        close(iDataSock);
        fclose(fp);
        //        HLOG("%s",er)
        return iRet;
    }

    //send local file
    while (llSentSize < llFileSize)
    {
        if ((llRet = llFileSize - llSentSize) > CMD_BUF_SIZE * 16)
        {
            llRet = CMD_BUF_SIZE * 16;
        }
        //read file and upload
        llRet = fread(arrchBuf, sizeof(char), llRet, fp);
        if ((llRet = SendFullPacket(iDataSock, arrchBuf, llRet)) <= 0)
        {
            break;
        }
        llSentSize += llRet;
    }
    fclose(fp);
    close(iDataSock);

    //get ftp server return code
    if ((iRet = GetRetCode(iCtrlSock, 226)))
    {
        return iRet;
    }

    //if postfix, rename file name
    if (pchPostfix != NULL && strlen(pchPostfix) > 0)
    {
        memset(arrchBuf, 0, sizeof(arrchBuf));
        //replace postfix
        if(iPostfixFlag == 0)
        {
            memset(arrchTmp,0,sizeof(arrchTmp));
            pchStr = strrchr(pchFtpFileName,'.');
            memcpy(arrchTmp,pchFtpFileName,pchStr - pchFtpFileName);
        }
        else
        {
            strcpy(arrchTmp,pchFtpFileName);
        }
        strcat(arrchTmp,pchPostfix);
        sprintf(arrchBuf, "RNFR %s", arrchTmp);
        if ((iRet = SendFtpCmd(iCtrlSock, arrchBuf)))
        {
            return iRet;
        }

        memset(arrchBuf, 0, sizeof(arrchBuf));
        sprintf(arrchBuf, "RNTO %s", pchFtpFileName);
        if ((iRet = SendFtpCmd(iCtrlSock, arrchBuf)))
        {
            return iRet;
        }
    }

    //change to previous ftp directory
    for (i=0; i<iLevel; i++)
    {
        iRet = SendFtpCmd(iCtrlSock, "CDUP");
    }

    //send failed if filesize greater than zero
    if (llSentSize < llFileSize)
    {
        return -5;
    }

    return 0;
}

/*
 * Upload
 * pchLocalPath: local file path, supoort like "xx", "./xx", "xx/"
 * pchFtpPath:   ftp dir path, support like "", "./", ".", "xx", "xx/"
 * pchIP:        IP address
 * iCtrlSock:    ctrl socket
 * lPos:         file position
 * pchPostfix:   transport file whether using postfix
 * iPostfixFlag  postfix flag (0 replace 1 postfix)
 * return:       success: 0
 *
 * Upload a file or folder to the ftp server's specific path.
 *
*/
int Upload2(const char *pchLocalPath, const char *pchLocalIP,const char *pchFtpPath, const char *pchIP, int iCtrlSock, long lPos, const char *pchPostfix ,const int iPostfixFlag,const int iDataTransType)
{
    char arrchBuf[CMD_BUF_SIZE * 16];  //缓冲区
    char arrchFtpPath[CMD_BUF_SIZE];   //FTP路径
    char arrchLocalPath[CMD_BUF_SIZE]; //本地路径
    char arrchTmp[CMD_BUF_SIZE];       //临时缓冲区
    char *pchTok;                      //临时指针
    char *pchFtpFileName;              //FTP文件名
    char *pchOuter;                    //指针
    char *pchStr;                      //字符指针
    int iRet;                          //返回值
    long long llRet;                   //返回值
    int iDataSock = -1;                     //数据套接字
    int iSock = -1;
    int iLevel = 0;                    //FTP级别
    int i;                             //循环变量
    long long llFileSize;              //文件大小
    long long llSentSize = 0;          //发送大小
    FILE *fp = NULL;                   //文件指针
    DIR                  *pDp;         //目录指针
    struct dirent        *pstruDir;    //目录结构指针
    struct stat          struStat;     //文件状态指针



    //validate input paras, only support upload dir and regular file
    stat(pchLocalPath, &struStat);
    if (pchFtpPath == NULL || pchIP == NULL || lPos < 0 || iCtrlSock < 2 ||
            (!S_ISDIR(struStat.st_mode) && !S_ISREG(struStat.st_mode)))
    {
        return -1;
    }

    //standard ftp remote dir path
    iRet = strlen(pchFtpPath);
    memcpy(arrchFtpPath, pchFtpPath, iRet);
    arrchFtpPath[iRet] = 0;
    if (arrchFtpPath[iRet - 1] == '/')
        arrchFtpPath[iRet - 1] = 0;

    //standard local path
    iRet = strlen(pchLocalPath);
    memcpy(arrchLocalPath, pchLocalPath, iRet);
    arrchLocalPath[iRet] = 0;
    if (arrchLocalPath[iRet - 1] == '/')
        arrchLocalPath[iRet - 1] = 0;

    //if local path is dir, then upload dir
    if (S_ISDIR(struStat.st_mode))
    {
        //open local dir
        if ((pDp = opendir(pchLocalPath)) == NULL)
        {
            return -2;
        }

        //create ftp dir path
        pchTok = strrchr(arrchLocalPath, '/');
        strcat(arrchFtpPath, "/");
        strcat(arrchFtpPath, pchTok != NULL ? pchTok + 1 : arrchLocalPath);

        //read local dir information and upload one by one
        while ((pstruDir = readdir(pDp)) != NULL)
        {
            //ignore folder . and ..
            if (!strcmp(pstruDir->d_name, ".") || !strcmp(pstruDir->d_name, ".."))
            {
                continue;
            }
            memset(arrchBuf, 0, sizeof(arrchBuf));
            sprintf(arrchBuf, "%s/%s", arrchLocalPath, pstruDir->d_name);

            //upload file or dir
            if ((iRet = Upload2(arrchBuf,pchLocalIP,arrchFtpPath, pchIP, iCtrlSock, 0, pchPostfix,iPostfixFlag,iDataTransType)))
            {
                closedir(pDp);
                return iRet;
            }
        }
        closedir(pDp);
        return 0;
    }

    //upload regular file
    if (strcmp(arrchFtpPath, "") && strcmp(arrchFtpPath, "."))
    {
        strcat(arrchFtpPath, "/");
    }

    //deal ftp path like "", "." or "dir_name"
    pchTok = strrchr(arrchLocalPath, '/');
    strcat(arrchFtpPath, pchTok != NULL ? pchTok + 1 : arrchLocalPath);

    //get ftp dir path
    memset(arrchBuf, 0, sizeof(arrchBuf));
    pchFtpFileName = strrchr(arrchFtpPath, '/');
    pchFtpFileName = pchFtpFileName == NULL ? arrchFtpPath : pchFtpFileName + 1;
    memcpy(arrchBuf, arrchFtpPath, pchFtpFileName - arrchFtpPath);
    pchStr = arrchBuf;
    //if ftp dir doesn't exist, then create it
    while ((pchTok = strtok_r(pchStr, "/", &pchOuter)) != NULL)
    {
        //ignore path name .
        if (pchTok[0] == '.')
        {
            pchStr = NULL;
            continue;
        }
        //try enter ftp dir
        sprintf(arrchTmp, "CWD %s", pchTok);
        if (SendFtpCmd(iCtrlSock, arrchTmp))
        {
            //create ftp dir
            sprintf(arrchTmp, "MKD %s", pchTok);
            if ((iRet = SendFtpCmd(iCtrlSock, arrchTmp)))
                return iRet;

            //enter ftp dir
            sprintf(arrchTmp, "CWD %s", pchTok);
            if ((iRet = SendFtpCmd(iCtrlSock, arrchTmp)))
                return iRet;
        }
        pchStr = NULL;
        iLevel++; //ftp dir path depth
    }


    //change transfer mode to binary
    if ((iRet = SendFtpCmd(iCtrlSock, "TYPE I")))
    {
        return iRet;
    }

    //it`s up to callers to use PASV or PORT mode. by hhd 20141014
    if(iDataTransType == PASV_MODE)//PASV
    {
        //create data socket connection.add judgement by hhd 20131122
        if((iDataSock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            return -3;
        }
        //connect ftp server using PASV mode
        memset(arrchTmp,0,sizeof(arrchTmp));
        if ((iRet = SendPASVFtpCmd(iCtrlSock, "PASV",arrchTmp)) < 0)
        {
            return iRet;
        }

        if (0 > CreateConnection(iDataSock, arrchTmp, iRet))
        {
            close(iDataSock);
            return -4;
        }
    }
    else if(iDataTransType == PORT_MODE)//PORT
    {
        //add by hhd 20141022
        if((iSock = PORTModeToServer(iCtrlSock,pchLocalIP)) < 0)
        {
            close(iSock);
            return iSock;
        }
    }
    //error
    else
    {
        return -5;
    }

    //    //change transfer mode to binary
    //    if ((iRet = SendFtpCmd(iCtrlSock, "TYPE I")))
    //    {
    //        close(iDataSock);
    //        return iRet;
    //    }

    //    //open local file and get its size
    //    if ((fp = fopen(pchLocalPath, "r")) == NULL)
    //    {
    //        close(iDataSock);
    //        return -6;
    //    }



    //send STOR command to ftp server
    if (!lPos)
        sprintf(arrchBuf, "STOR %s", pchFtpFileName);
    else
        sprintf(arrchBuf, "APPE %s", pchFtpFileName);

    //postfix upload if exist
    if (pchPostfix != NULL && strlen(pchPostfix) > 0)
    {
        //replace postfix
        if(iPostfixFlag == 0)
        {
            memset(arrchTmp,0,sizeof(arrchTmp));
            pchStr = strrchr(arrchBuf,'.');
            memcpy(arrchTmp,arrchBuf,pchStr - arrchBuf);
            memset(arrchBuf,0,sizeof(arrchBuf));
            memcpy(arrchBuf,arrchTmp,strlen(arrchTmp));
        }
        strcat(arrchBuf, pchPostfix);
    }

    if ((iRet = SendFtpCmd(iCtrlSock, arrchBuf)))
    {
        if(iDataTransType == PORT_MODE)//PORT
        {
            close(iSock);
        }
        return iRet;
    }

    if(iDataTransType == PORT_MODE)//PORT
    {
        if((iDataSock = AcceptConnection(iSock)) < 0)
        {
            close(iSock);
            return -6;
        }
    }

    //open local file and get its size
    if ((fp = fopen(pchLocalPath, "r")) == NULL)
    {
        close(iDataSock);
        if(iDataTransType == PORT_MODE)//PORT
        {
            close(iSock);
        }
        return -7;
    }
    //fseek to the end of the file
    fseek(fp, 0L, SEEK_END);
    llFileSize = ftell(fp);
    llFileSize -= lPos;
    fseek(fp, lPos, SEEK_SET);

    //send local file
    while (llSentSize < llFileSize)
    {
        if ((llRet = llFileSize - llSentSize) > CMD_BUF_SIZE * 16)
        {
            llRet = CMD_BUF_SIZE * 16;
        }
        //read file and upload
        llRet = fread(arrchBuf, sizeof(char), llRet, fp);
        if ((llRet = SendFullPacket(iDataSock, arrchBuf, llRet)) <= 0)
        {
            break;
        }
        llSentSize += llRet;
    }
    fclose(fp);
    close(iDataSock);
    if(iDataTransType == PORT_MODE)//PORT
    {
        close(iSock);
    }

    //get ftp server return code
    if ((iRet = GetRetCode(iCtrlSock, 226)))
    {
        return iRet;
    }

    //if postfix, rename file name
    if (pchPostfix != NULL && strlen(pchPostfix) > 0)
    {
        memset(arrchBuf, 0, sizeof(arrchBuf));
        //replace postfix
        if(iPostfixFlag == 0)
        {
            memset(arrchTmp,0,sizeof(arrchTmp));
            pchStr = strrchr(pchFtpFileName,'.');
            memcpy(arrchTmp,pchFtpFileName,pchStr - pchFtpFileName);
        }
        else
        {
            strcpy(arrchTmp,pchFtpFileName);
        }
        strcat(arrchTmp,pchPostfix);
        sprintf(arrchBuf, "RNFR %s", arrchTmp);
        if ((iRet = SendFtpCmd(iCtrlSock, arrchBuf)))
        {
            return iRet;
        }

        memset(arrchBuf, 0, sizeof(arrchBuf));
        sprintf(arrchBuf, "RNTO %s", pchFtpFileName);
        if ((iRet = SendFtpCmd(iCtrlSock, arrchBuf)))
        {
            return iRet;
        }
    }

    //change to previous ftp directory
    for (i=0; i<iLevel; i++)
    {
        iRet = SendFtpCmd(iCtrlSock, "CDUP");
    }

    //send failed if filesize greater than zero
    if (llSentSize < llFileSize)
    {
        return -8;
    }

    return 0;
}

/*
 * UploadConn
 * pchLocalPath: local file path
 * pchFtpPath:   ftp dir path, support like "", "./", ".", "xx", "xx/"
 * pchIP:        IP address
 * iPort:        ftp port
 * pchUser:      ftp user
 * pchPwd:       ftp password
 * lPos:         file position
 * pchPostfix:   transport file whether using postfix
 *
 * return:       success: 0
 *
 * Upload a file or folder to the ftp server's specific path.
 *
*/
int UploadConn2(const char *pchLocalPath,const char *pchLocalIP,const FTP_STRUCT *pstruRemoteFTP,const char *pchPostfix,const int iPostfixFlag,const int iDataTransType)
{
    int iRet;       //
    int iCtrlSock;  //

    //
    iCtrlSock = socket(AF_INET, SOCK_STREAM, 0);
    //
    if (ConnectFtp(iCtrlSock, pstruRemoteFTP->arrchIP, pstruRemoteFTP->iPort, pstruRemoteFTP->arrchUserName, pstruRemoteFTP->arrchPassword) < 0)
    {
        close(iCtrlSock);
        return -1;
    }
    iRet = Upload2(pchLocalPath,pchLocalIP,pstruRemoteFTP->arrchPath, pstruRemoteFTP->arrchIP, iCtrlSock, pstruRemoteFTP->lPos, pchPostfix,iPostfixFlag,iDataTransType);
    SendFtpCmd(iCtrlSock, "QUIT");
    close(iCtrlSock);
    return iRet;
}

/*
 * UploadConn
 * pchLocalPath: local file path
 * pchFtpPath:   ftp dir path, support like "", "./", ".", "xx", "xx/"
 * pchIP:        IP address
 * iPort:        ftp port
 * pchUser:      ftp user
 * pchPwd:       ftp password
 * lPos:         file position
 * pchPostfix:   transport file whether using postfix
 *
 * return:       success: 0
 *
 * Upload a file or folder to the ftp server's specific path.
 *
*/
int UploadConn(const char *pchLocalPath, const char *pchFtpPath, const char *pchIP,
               int iPort, const char *pchUser, const char *pchPwd, long lPos, const char *pchPostfix,const int iPostfixFlag)
{
    int iRet;
    int iCtrlSock = socket(AF_INET, SOCK_STREAM, 0);
    if (ConnectFtp(iCtrlSock, pchIP, iPort, pchUser, pchPwd) < 0)
    {

        close(iCtrlSock);
        HLOG("connect ftpserver failed");
        return -1;
    }
    iRet = Upload(pchLocalPath, pchFtpPath, pchIP, iCtrlSock, lPos, pchPostfix,iPostfixFlag);
    SendFtpCmd(iCtrlSock, "QUIT");
    close(iCtrlSock);
    return iRet;
}

/*
 * UploadConn
 * pchLocalPath: local file path
 * pchFtpPath:   ftp dir path, ""表示当前文件夹,"work/"或 "work"表示ftp登录目录的下几级文件夹，支持远端自动创建,支持递归文件夹发送
 * pchIP:        IP address
 * iPort:        ftp port
 * pchUser:      ftp user
 * pchPwd:       ftp password
 * lPos:         file position
 * pchPostfix:   transport file whether using postfix,默认NULL
 * iPostfixFlag: 默认0
 *pchSrcIP  local ip
 * return:       success: 0
 *
 * Upload a file or folder to the ftp server's specific path.
 *
*/
int UploadConnBindSrcAddr(const char *pchLocalPath, const char *pchFtpPath, const char *pchIP,
                          int iPort, const char *pchUser, const char *pchPwd, long lPos, const char *pchPostfix,const int iPostfixFlag, const char *pchSrcIP)
{
    int iRet;
    int iCtrlSock = socket(AF_INET, SOCK_STREAM, 0);

    //bind src addr,add by panj 20160117
    if(pchSrcIP != NULL)
    {
        struct sockaddr_in sin;
        sin.sin_family = AF_INET;
        sin.sin_addr.s_addr = inet_addr(pchSrcIP);
        sin.sin_port = INADDR_ANY;
        bind(iCtrlSock,(struct sockaddr *)&sin, sizeof(struct sockaddr_in));
    }

    if (ConnectFtp(iCtrlSock, pchIP, iPort, pchUser, pchPwd) < 0)
    {

        close(iCtrlSock);
        return -1;
    }
    iRet = UploadBindSrcAddr(pchLocalPath, pchFtpPath, pchIP, iCtrlSock, lPos, pchPostfix,iPostfixFlag,pchSrcIP);
    SendFtpCmd(iCtrlSock, "QUIT");
    close(iCtrlSock);
    return iRet;
}

/*
 * Download
 * pchLocalPath: local file path, support like "", "./", ".", "xx", "xx/"
 * pchFtpPath:   ftp dir path
 * pchIP:        IP address
 * iCtrlSock:    ctrl socket
 * lPos:         file position
 * pchPostfix:   transport file whether using postfix
 *
 * return:       success: 0
 *
 * Download a file or folder from the ftp server's specific path.
 *
*/
int Download(const char *pchLocalPath, const char *pchFtpPath, const char *pchIP, int iCtrlSock, long lPos, const char *pchPostfix)
{
    char arrchBuf[CMD_BUF_SIZE * 16];  //memory buffer
    char arrchFtpPath[CMD_BUF_SIZE];   //remote path
    char arrchLocalPath[CMD_BUF_SIZE]; //local path
    char arrchTmp[CMD_BUF_SIZE];       //tmp buffer
    char *pchTok;                      //tmp pointer
    int iRet;                          //return value
    int iDataSock = 0;                 //data socket
    int iItemNum;                      //LIST command info num
    int i;                             //loop varibale
    int iCWD = 0;
    long lFileSize;                    //local file size
    long lSentSize = 0;                //sent size
    FILE *fp;                          //file pointer
    FTP_LIST_ITEM_STRUCT *pstruList = NULL;

    //validate input paras, only support upload dir and regular file
    if (pchFtpPath == NULL || pchIP == NULL || pchLocalPath == NULL || lPos < 0)
    {
        HLOG( "NULL");
        return -1;
    }

    //standard ftp remote dir path
    iRet = strlen(pchFtpPath);
    memcpy(arrchFtpPath, pchFtpPath, iRet);

    arrchFtpPath[iRet] = 0;
    if (arrchFtpPath[iRet - 1] == '/')
        arrchFtpPath[iRet - 1] = 0;

    //standard local path
    iRet = strlen(pchLocalPath);
    memcpy(arrchLocalPath, pchLocalPath, iRet);
    arrchLocalPath[iRet] = 0;
    if (strcmp(arrchLocalPath, "") && arrchLocalPath[iRet - 1] != '/')
    {
        strcat(arrchLocalPath, "/");
        arrchLocalPath[iRet + 1] = 0;
    }
    //get local file/folder path
    pchTok = strrchr(arrchFtpPath, '/');
    strcat(arrchLocalPath, pchTok == NULL ? arrchFtpPath : pchTok + 1);

    //get information about ftp path
    if ((iItemNum = ParseFtpListCmd(iCtrlSock, arrchFtpPath, pchIP, &pstruList)) <= 0)
    {
        HLOG( "ParseFtpListCmd %d, %s", iItemNum, arrchFtpPath);
        return -20; //test 20131104
    }

    pchTok = strrchr(arrchFtpPath, '/');
    pchTok = pchTok == NULL ? arrchFtpPath : pchTok + 1;

    //if ftp path is directory, enter the dir.
    if (iItemNum > 1 || pstruList[0].arrchAttributes[0] == 'd'
            || strcmp(pchTok, pstruList[0].arrchFileName))
    {
        //enter ftp dir
        memset(arrchTmp, 0, sizeof(arrchTmp));
        sprintf(arrchTmp, "CWD %s", arrchFtpPath);
        if ((iRet = SendFtpCmd(iCtrlSock, arrchTmp)))
        {
            HLOG( "SendFtpCmd %s", arrchTmp);
            goto DOWNLOAD_EXIT;
        }

        //download all files and folders
        for (i=0; i<iItemNum; i++)
        {
            if ((iRet = Download(arrchLocalPath, pstruList[i].arrchFileName, pchIP, iCtrlSock, 0, pchPostfix)) < 0)
            {
                HLOG( "SendFtpCmd %s", pstruList[i].arrchFileName);
                goto DOWNLOAD_EXIT;
            }
        }

        //return to previous directory
        iRet = SendFtpCmd(iCtrlSock, "CDUP");

        iRet = 0;
        HLOG( "SendFtpCmd %s", pstruList[i].arrchFileName);
        goto DOWNLOAD_EXIT;
    }
    else
    {
        pchTok = strrchr(arrchFtpPath, '/');
        if (pchTok != NULL)
        {
            //enter ftp dir
            memset(arrchTmp, 0, sizeof(arrchTmp));
            memcpy(arrchTmp, arrchFtpPath, pchTok - arrchFtpPath);
            memset(arrchBuf, 0, sizeof(arrchBuf));
            sprintf(arrchBuf, "CWD %s", arrchTmp);
            if ((iRet = SendFtpCmd(iCtrlSock, arrchBuf)))
            {
                HLOG( "SendFtpCmd %s", arrchBuf);
                goto DOWNLOAD_EXIT;
            }
            iCWD = 1;
        }
    }

    //connect ftp server using PASV mode
    memset(arrchTmp, 0, sizeof(arrchTmp));
    if ((iRet = SendPASVFtpCmd(iCtrlSock, "PASV",arrchTmp)) < 0)
    {
        HLOG( "SendFtpCmd %d", iRet);
        goto DOWNLOAD_EXIT;
    }

    //it`s up to callers to use PASV or PORT mode. by hhd 20141014



    //create data socket connection. add judgement by hhd 20131122
    if((iDataSock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        close(iDataSock);
        iRet = -2;
        HLOG("socket:%s",strerror(errno));
        goto DOWNLOAD_EXIT;
    }

    if (CreateConnection(iDataSock, arrchTmp, iRet) < 0)
    {
        iRet = -3;
        HLOG( "SendFtpCmd %d", iRet);
        goto DOWNLOAD_EXIT;
    }

    //change transfer mode to binary
    if ((iRet = SendFtpCmd(iCtrlSock, "TYPE I")) < 0)
    {
        HLOG( "SendFtpCmd %d", iRet);
        goto DOWNLOAD_EXIT;
    }

    //support REST command
    if (lPos > 0)
    {
        sprintf(arrchBuf, "REST %ld", lPos);
        if ((iRet = SendFtpCmd(iCtrlSock, arrchBuf)) < 0)
        {
            HLOG( "SendFtpCmd %s", arrchBuf);
            goto DOWNLOAD_EXIT;
        }
    }

    //send RETR command to ftp server
    sprintf(arrchBuf, "RETR %s", pstruList[0].arrchFileName);
    if ((iRet = SendFtpCmd(iCtrlSock, arrchBuf)) < 0)
    {
        HLOG( "SendFtpCmd %s", arrchBuf);
        goto DOWNLOAD_EXIT;
    }

    //create local folder if it doesn't exist
    if ((pchTok = strrchr(arrchLocalPath, '/')) != NULL)
    {
        memset(arrchTmp, 0, sizeof(arrchTmp));
        memcpy(arrchTmp, arrchLocalPath, pchTok - arrchLocalPath + 1);
        if (IsDirExists(arrchTmp, "") == -1)
        {
            BuildDirectory(arrchTmp);
        }
    }

    memset(arrchBuf, 0, sizeof(arrchBuf));
    strcpy(arrchBuf, arrchLocalPath);
    //postfix upload if exist
    if (pchPostfix != NULL && strlen(pchPostfix) > 0)
        strcat(arrchBuf, pchPostfix);

    //open local file for write
    if ((fp = fopen(arrchBuf, "ab+")) == NULL)
    {
        iRet = -3;
        fclose(fp); //add 20131122
        goto DOWNLOAD_EXIT;
    }

    lFileSize = atol(pstruList[0].arrchSize) - lPos;
    //receive all the data
    while (lSentSize < lFileSize)
    {
        iRet = RecvPacket(iDataSock, arrchTmp, sizeof(arrchTmp));
        if (iRet <= 0)
            break;
        fwrite(arrchTmp, sizeof(char), iRet, fp);
        lSentSize += iRet;
    }
    fflush(fp);
    fclose(fp);
    close(iDataSock);
    iDataSock = 0;

    //get ftp server return code
    if ((iRet = GetRetCode(iCtrlSock, 226)))
    {
        HLOG( "SendFtpCmd %d", iRet);
        goto DOWNLOAD_EXIT;
    }

    if (pchPostfix != NULL && strlen(pchPostfix) > 0)
    {
        memset(arrchBuf, 0, sizeof(arrchBuf));
        strcpy(arrchBuf, arrchLocalPath);
        strcat(arrchBuf, pchPostfix);
        rename(arrchBuf, arrchLocalPath);
    }

    //receive failed if filesize greater than zero
    iRet = lSentSize < lFileSize ? -4 : 0;

DOWNLOAD_EXIT:

    if (iCWD)
    {
        /*
        pchTok = strrchr(arrchFtpPath, '/');
        memset(arrchTmp, 0, sizeof(arrchTmp));
        strcpy(arrchTmp, "CWD /");
        memcpy(arrchTmp + strlen(arrchTmp), arrchFtpPath, pchTok - arrchFtpPath + 1);
        HLOG( "%s, %s", arrchTmp, pchTok);
        iRet = SendFtpCmd(iCtrlSock, arrchTmp);
        */
        pchTok = arrchFtpPath;
        while (NULL != (pchTok = strstr(pchTok, "/")))
        {
            iRet = SendFtpCmd(iCtrlSock, "CDUP");
            pchTok++;
        }
    }
    //    if (iDataSock)
    //    {
    //    close(iDataSock);
    //    }
    if (pstruList != NULL)
    {
        free(pstruList);
        pstruList = NULL;
    }
    return iRet;
}

/*
 * DownloadConn
 * pchLocalPath: local file path, support like "", "./", ".", "xx", "xx/"
 * pchFtpPath:   ftp dir path
 * pchIP:        IP address
 * iPort:        ftp port
 * pchUser:      ftp user
 * pchPwd:       ftp password
 * lPos:         file position
 * pchPostfix:   transport file whether using postfix
 *
 * return:       success: 0
 *
 * Download a file or folder from the ftp server's specific path.
 *
*/
int DownloadConn(const char *pchLocalPath, const char *pchFtpPath, const char *pchIP,
                 int iPort, const char *pchUser, const char *pchPwd, long lPos, const char *pchPostfix)
{
    //    HLOG("FTP Path: %s.", pchFtpPath);

    int iRet;
    int iCtrlSock = socket(AF_INET, SOCK_STREAM, 0);
    if (ConnectFtp(iCtrlSock, pchIP, iPort, pchUser, pchPwd) < 0)
    {
        close(iCtrlSock);
        return -1;
    }
    iRet = Download(pchLocalPath, pchFtpPath, pchIP, iCtrlSock, lPos, pchPostfix);

    SendFtpCmd(iCtrlSock, "QUIT");
    close(iCtrlSock);
    printf("DownloadConn\n");
    return iRet;
}

/*
 * MonthToInt
 * pchMonth：月份字符串，如Jan
 * 返回值：  月份对应的数字， 失败返回值小于0
 *
 * 将月份转换成整数
*/
int MonthToInt(const char *pchMonth)
{
    int iRet;
    if (!strcmp(pchMonth, "Jan"))
    {
        iRet = 1;
    }
    else if (!strcmp(pchMonth, "Feb"))
    {
        iRet = 2;
    }
    else if (!strcmp(pchMonth, "Mar"))
    {
        iRet = 3;
    }
    else if (!strcmp(pchMonth, "Apr"))
    {
        iRet = 4;
    }
    else if (!strcmp(pchMonth, "May"))
    {
        iRet = 5;
    }
    else if (!strcmp(pchMonth, "Jun"))
    {
        iRet = 6;
    }
    else if (!strcmp(pchMonth, "Jul"))
    {
        iRet = 7;
    }
    else if (!strcmp(pchMonth, "Aug"))
    {
        iRet = 8;
    }
    else if (!strcmp(pchMonth, "Sep"))
    {
        iRet = 9;
    }
    else if (!strcmp(pchMonth, "Oct"))
    {
        iRet = 10;
    }
    else if (!strcmp(pchMonth, "Nov"))
    {
        iRet = 11;
    }
    else if (!strcmp(pchMonth, "Dec"))
    {
        iRet = 12;
    }
    else
    {
        iRet = -1;
    }
    return iRet;
}

/*
 * ParseVsftpLog
 * pchLogPath： 日志路径
 * pchFileName: 查找的文件名
 * pstruLog:    日志结构体
 * 返回值：  成功返回0，失败返回值小于0
 *
 * 解析VSFTP日志信息
*/
int ParseVsftpLog(const char *pchLogPath, const char *pchFileName, VSFTP_LOG_STRUCT *pstruLog)
{
    char arrchBuf[1024];
    char *pchTok;
    char *pchStr;
    char *pchTmp;
    int i = 0;
    FILE *fpOut;

    memset(arrchBuf, 0, sizeof(arrchBuf));
    sprintf(arrchBuf, "sed -n -e '/%s/p' %s", pchFileName, pchLogPath);
    if ((fpOut = popen(arrchBuf, "r")) == NULL)
    {
        return -1;
    }
    memset(arrchBuf, 0, sizeof(arrchBuf));
    fgets(arrchBuf, sizeof(arrchBuf), fpOut);
    pchStr = arrchBuf;

    if ((pchTok = strtok_r(pchStr, " ",&pchTmp)) == NULL)
    {
        pclose(fpOut);
        return -2;
    }

    while (pchTok != NULL)
    {
        switch (i)
        {
        case 1:
            snprintf(pstruLog->arrchDate + 5, 4, "%02d-", MonthToInt(pchTok)); break;
        case 2:
            snprintf(pstruLog->arrchDate + 8, 4, "%02d ", atoi(pchTok)); break;
        case 3:
            strcpy(pstruLog->arrchDate + 11, pchTok); break;
        case 4:
            memcpy(pstruLog->arrchDate, pchTok, 4);
            pstruLog->arrchDate[4] = '-';
            break;
        case 5:
            pstruLog->iTransTime = atoi(pchTok); break;
        case 6:
            strcpy(pstruLog->arrchSrcIP, pchTok); break;
        case 7:
            pstruLog->oFileSize = atol(pchTok); break;
        case 8:
            strcpy(pstruLog->arrchFileName, pchTok); break;
        case 9:
            pstruLog->chTransMode = pchTok[0]; break;
        case 10:
            pstruLog->chBehavior = pchTok[0]; break;
        case 11:
            pstruLog->chDescription = pchTok[0]; break;
        case 12:
            pstruLog->chVisitMode = pchTok[0]; break;
        case 13:
            strcpy(pstruLog->arrchLoginName, pchTok); break;
        case 14:
            strcpy(pstruLog->arrchFtpName, pchTok); break;
        case 15:
            pstruLog->chAuthenticationMode = pchTok[0]; break;
        case 16:
            pstruLog->chFlag = pchTok[0]; break;
        case 17:
            pstruLog->chStatus = pchTok[0]; break;
        default: break;
        }
        i++;
        pchTok = strtok_r(NULL, " ",&pchTmp);
    }
    pclose(fpOut);
    return 0;
}

int PORTModeToServer(const int iCtrlSock,const char *pchLocalIP)
{
    int iSock;
    int iPort;
    int iRet;
    char arrchBuf[128];
    char arrchIPSplit[4][4];
    memset(arrchBuf,0,sizeof(arrchBuf));

    if((iSock = InitTcpSocket()) < 0)
    {
        return -1;
    }

    //初始化随机数种子
    srand((unsigned)time(NULL));
    //获取随机端口
    iPort = rand() % (PORT_MAX_VAL - PORT_MIN_VAL) + PORT_MIN_VAL;

    if((iRet = BindSocket(iSock,pchLocalIP,iPort)) < 0)
    {
        return iRet;
    }
    memset(arrchIPSplit,0,sizeof(arrchIPSplit));
    Ip_Split(pchLocalIP,".",arrchIPSplit);

    sprintf(arrchBuf,"PORT %s,%s,%s,%s,%d,%d",arrchIPSplit[0],arrchIPSplit[1],arrchIPSplit[2],arrchIPSplit[3],iPort / 256,iPort % 256);
    //connect ftp server using PORT mode
    if ((iRet = SendFtpCmd(iCtrlSock,arrchBuf)))
    {
        return iRet;
    }

    return iSock;
}

/*
 *函数名：Extract_Tokens
 *参数：String
 *     delim
 *     Single
 *返回值：分离后的数目
 *功能：分离字符串
 */
int Ip_Split(const char *String, char *delim, char Single[4][4])
{
    char *temp;             //
    char *pchOuter;         //
    char *pchStr;           //
    char arrchTemp[256];    //
    int  i=0;               //

    //
    temp = NULL;
    pchOuter = NULL;
    memset(arrchTemp, 0, sizeof(arrchTemp));
    strcpy(arrchTemp, String);
    pchStr = arrchTemp;

    //
    while((temp = strtok_r(pchStr, delim, &pchOuter)) != NULL)
    {
        memset(Single[i], 0, sizeof(Single[i]));
        strcpy(Single[i], temp);
        i++;
        pchStr = NULL;
    }
    //返回
    return i;
}

/*
 *函数：GetServerIP
 *参数：pchOrgin   PASV返回值
 *     pchIP      提取出来的IP
 *返回值：0
 *功能：提取PASV命令返回值中的地址信息
 */
int GetServerIP(const char *pchOrgin,char *pchIP)
{
    char arrchIP[17];
    char arrchTmp[5];
    char *pchIndex;
    char *pchTemp;
    int i;

    memset(arrchIP,0,sizeof(arrchIP));

    pchIndex = strstr(pchOrgin,"(");
    pchTemp = pchIndex + 1;

    for(i = 0;i < 4;i++)
    {
        pchIndex = strstr(pchTemp,",");
        memset(arrchTmp,0,sizeof(arrchTmp));
        memcpy(arrchTmp,pchTemp,pchIndex - pchTemp);
        sprintf(arrchIP + strlen(arrchIP),"%s.",arrchTmp);
        pchTemp = pchIndex + 1;
    }

    arrchIP[strlen(arrchIP) - 1] = 0;

    strcpy(pchIP,arrchIP);

    return 0;
}


int CheckFileExist(const char *pchIP, int iPort, const char *pchUser, const char *pchPwd, const char *pRemoteFile)
{
    int iCtrlSock ;
    int iRet = 0;

    char arrchBuf[256];
    char arrchSize[100];

    memset(arrchBuf, 0, sizeof(arrchBuf));

    //init sock
    iCtrlSock = InitTcpSocket();
    if(iCtrlSock < 0)
    {
        return -1;
    }

    if (ConnectFtp(iCtrlSock, pchIP, iPort, pchUser, pchPwd) < 0)
    {
        close(iCtrlSock);
        HLOG( "cannot connect dest ftp server:%s", pchIP);
        return -1;
    }
    sprintf(arrchBuf, "SIZE %s\r\n", pRemoteFile);
    //发送指令，获取文件大小
    iRet = SendPacket(iCtrlSock, arrchBuf, strlen(arrchBuf));
    if(iRet <= 0)
    {
        close(iCtrlSock);
        return -1;
    }
    memset(arrchBuf, 0, sizeof(arrchBuf));
    //接收FTP服务器的返回代码，如果接收失败，返回错误代码
    iRet = RecvPacket(iCtrlSock, arrchBuf, CMD_BUF_SIZE);
    if(iRet <= 0)
    {
        close(iCtrlSock);

        return -1;
    }
    arrchBuf[iRet] = 0;
    memset(arrchSize,0,sizeof(arrchSize));
    memcpy(arrchSize, arrchBuf , 3);

    if(atoi(arrchSize) == 213)//如果返回码为213，则认为成功返回远程服务器文件大小
    {
        close(iCtrlSock);
        return 0;
    }
    else//如果未获取到文件大小，则认为远程服务器上没有此文件
    {
        close(iCtrlSock);
        return -1;
    }
}

int CheckDESCFileExistBindSrcAddr(const char *pchIP, int iPort, const char *pchUser, const char *pchPwd, const char *pRemoteFile, const char *pchSrcIP)
{
    int iCtrlSock ;
    int iRet = 0;

    char arrchBuf[256];
    char arrchSize[100];
    char acfilesize[100];
    memset(arrchBuf, 0, sizeof(arrchBuf));
    memset(acfilesize, 0, sizeof(acfilesize));
    //init sock
    iCtrlSock = InitTcpSocket();
    if(iCtrlSock < 0)
    {
        return -1;
    }

    //bind src addr,add by panj 20160117
    if(pchSrcIP != NULL)
    {
        struct sockaddr_in sin;
        sin.sin_family = AF_INET;
        sin.sin_addr.s_addr = inet_addr(pchSrcIP);
        sin.sin_port = INADDR_ANY;
        bind(iCtrlSock,(struct sockaddr *)&sin, sizeof(struct sockaddr_in));
    }

    if (ConnectFtp(iCtrlSock, pchIP, iPort, pchUser, pchPwd) < 0)
    {
        close(iCtrlSock);
        HLOG( "cannot connect dest ftp server:%s", pchIP);
        return -1;
    }
    sprintf(arrchBuf, "SIZE %s\r\n", pRemoteFile);
    //发送指令，获取文件大小
    iRet = SendPacket(iCtrlSock, arrchBuf, strlen(arrchBuf));
    if(iRet <= 0)
    {
        close(iCtrlSock);
        return -1;
    }
    memset(arrchBuf, 0, sizeof(arrchBuf));
    //接收FTP服务器的返回代码，如果接收失败，返回错误代码
    iRet = RecvPacket(iCtrlSock, arrchBuf, CMD_BUF_SIZE);
    if(iRet <= 0)
    {
        close(iCtrlSock);
        return -1;
    }
    arrchBuf[iRet] = 0;
    memset(arrchSize,0,sizeof(arrchSize));
    memcpy(arrchSize, arrchBuf , 3);
    HLOG("***********************************arrchSize = %d",atoi(arrchSize));
    HLOG("***********************************arrchBuf = %s",arrchBuf);
    memcpy(acfilesize,arrchBuf+4,iRet-3-1);
    if((atoi(arrchSize) == 213)&&(atoi(acfilesize)!=0))//如果返回码为213，则认为成功返回远程服务器文件大小
    {
        close(iCtrlSock);
        return 0;
    }
    else//如果未获取到文件大小，则认为远程服务器上没有此文件
    {
        close(iCtrlSock);
        return -1;
    }
}

int CheckFileExistBindSrcAddr(const char *pchIP, int iPort, const char *pchUser, const char *pchPwd, const char *pRemoteFile, const char *pchSrcIP)
{
    int iCtrlSock ;
    int iRet = 0;

    char arrchBuf[256];
    char arrchSize[100];
    memset(arrchBuf, 0, sizeof(arrchBuf));

    //init sock
    iCtrlSock = InitTcpSocket();
    if(iCtrlSock < 0)
    {
        return -1;
    }

    //bind src addr,add by panj 20160117
    if(pchSrcIP != NULL)
    {
        struct sockaddr_in sin;
        sin.sin_family = AF_INET;
        sin.sin_addr.s_addr = inet_addr(pchSrcIP);
        sin.sin_port = INADDR_ANY;
        bind(iCtrlSock,(struct sockaddr *)&sin, sizeof(struct sockaddr_in));
    }

    if (ConnectFtp(iCtrlSock, pchIP, iPort, pchUser, pchPwd) < 0)
    {
        close(iCtrlSock);
        HLOG( "cannot connect dest ftp server:%s", pchIP);
        return -1;
    }
    sprintf(arrchBuf, "SIZE %s\r\n", pRemoteFile);
    //发送指令，获取文件大小
    iRet = SendPacket(iCtrlSock, arrchBuf, strlen(arrchBuf));
    if(iRet <= 0)
    {
        close(iCtrlSock);
        return -1;
    }
    memset(arrchBuf, 0, sizeof(arrchBuf));
    //接收FTP服务器的返回代码，如果接收失败，返回错误代码
    iRet = RecvPacket(iCtrlSock, arrchBuf, CMD_BUF_SIZE);
    if(iRet <= 0)
    {
        close(iCtrlSock);
        return -1;
    }
    arrchBuf[iRet] = 0;
    memset(arrchSize,0,sizeof(arrchSize));
    memcpy(arrchSize, arrchBuf , 3);
    HLOG("***********************************arrchSize = %d",atoi(arrchSize));
    HLOG("***********************************arrchBuf = %s",arrchBuf);
    if(atoi(arrchSize) == 213)//如果返回码为213，则认为成功返回远程服务器文件大小
    {
        close(iCtrlSock);
        return 0;
    }
    else//如果未获取到文件大小，则认为远程服务器上没有此文件
    {
        close(iCtrlSock);
        return -1;
    }
}
