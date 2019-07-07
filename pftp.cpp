#include "pftp.h"

pftp::pftp()
{
    this->strRealIp="";
    this->iPortPasv=-1;
}

//状态连接最后关,数据连接要即使关
pftp::~pftp()
{
    this->ptcpStatus->close();
    delete ptcpStatus;
}

int pftp::connect(string ip, string user, string pwd, int port)
{
    this->strIp=ip;
    this->strUser=user;
    this->strPwd=pwd;
    this->iPort=port;

    int iCtrlSock = socket(AF_INET, SOCK_STREAM, 0);
    if (ConnectFtp(iCtrlSock, ip.c_str(), port, user.c_str(), pwd.c_str()) < 0)
    {
        hlog(pstring()<<"connect"<<ip<<"fail");
        close(iCtrlSock);
        return -1;
    }

    //初始化
    this->ptcpStatus=new ptcp(iCtrlSock);
    hlog(*this->ptcpStatus);


    //realip先设置为连接的ip，默认虚拟ip
    this->strRealIp=ip;

    return 0;
}

int pftp::reconnect()
{
    while(this->connect(this->strIp,this->strUser,this->strPwd,this->iPort)<0)
    {
        hlog(pstring()<<"重连"<<this->strIp<<"中，请稍后...");
        sleep(1);
    }
    hlog(pstring()<<"重连"<<this->strIp<<"成功");
}
//超时时间30秒,在这里面设置的SendFtpCmd
int pftp::sendcmd(string cmd)
{
    return SendFtpCmd(this->ptcpStatus->sock,cmd.c_str());
}

int pftp::quit()
{
    return sendcmd("QUIT");
}

long long pftp::getLength(string path)
{
    string cmd="SIZE "+path+"\r\n";
    //    hlog(cmd);
    int iSendRes= SendPacket(this->ptcpStatus->sock, (void*)cmd.c_str(), cmd.size());
    if(iSendRes<0)
    {
        hlog(pstring()<<"发送获取远程ftp文件"<<path<<"大小命令失败");
        return iSendRes;;
    }
    //    hlog(iSendRes);
    char recv[256];
    bzero(recv,sizeof(recv));
    int iRecvRes=this->recvres(recv,256);
    if(iRecvRes<0)
    {
        hlog(pstring()<<"接收远程ftp文件"<<path<<"大小结果失败");
        return iRecvRes;
    }
    //    hlog(iRecvRes,recv);

    pliststring listres=pstring(recv).split(" \r\n");
    //    hlog(listres);

    //如果返回码为213，则任务成功返回远程服务器文件大小
    if(listres[0]=="213")
    {
        long long llres=atoll(listres[1].c_str());
        hlog(pstring()<<"获取远程ftp文件"<<path<<"大小结果成功："<<llres);
        return llres;
    }
    else
    {
        hlog(recv);
        return -5;
    }
}

int pftp::recvres(char *dataRecv, int len)
{
    return RecvPacket(this->ptcpStatus->sock,dataRecv,len);
}

int pftp::setPASV()
{
    int iRet=-99;
    char arrchTmp[256];
    memset(arrchTmp,0,sizeof(arrchTmp));

    //这个返回的iRet改为端口号了
    if ((iRet = SendPASVFtpCmd(this->ptcpStatus->sock, "PASV",arrchTmp)) < 0)
    {
        hlog("获取被动模式返回端口失败");
        return iRet;
    }
    HLOG("pasv port: %d",iRet);

    this->iPortPasv=iRet;
    this->strRealIp=string(arrchTmp);
    hlog(this->iPortPasv);
    hlog(this->strRealIp);

    return iRet;
}
//默认自带断点续传，无限次数，直到传完才返回
int pftp::upload(string strPathLocal, string strPathRemote)
{
    if(!plib::isExsist(strPathLocal))
    {
        hlog(pstring()<<"文件或路径"<<strPathLocal<<"不存在，请检查本地文件或文件夹");
        return -9;
    }

    //    pliststring lres=pstring(strPathLocal).split("/");
    if(strPathRemote[strPathRemote.size()-1]!='/')
        strPathRemote=strPathRemote+"/";
    //    pstring strRemote=strPathRemote+lres[lres.size()-1];
    //    hlog(strRemote);

    char arrchNowFile[256];
    bzero(arrchNowFile,sizeof(arrchNowFile));
    long long llpos=0;
    while(uploadNoReTrans(strPathLocal,strPathRemote,arrchNowFile,llpos)<0)
    {
        reconnect();
        hlog(arrchNowFile);
        pliststring lres=pstring(arrchNowFile).split("/");
        //在这要判断是否是目录，如果是目录，则要特殊处理
        if(plib::getPathType(strPathLocal)=="dir")
        {
            //要把本地目录也加上，先取文件夹名,文件夹名是最后strPathLocal的最后一个
            pliststring listLocalDir=pstring(strPathLocal).split("/");
            string strNameDir=listLocalDir[listLocalDir.size()-1];
            hlog(strNameDir);
            //然后从arrchNowFile-strPathLocal+文件夹名就是远程下边应该有的名字
            pliststring listFileReturn=pstring(arrchNowFile).split("/");
            pliststring listres;
            for(int i=listLocalDir.size();i<listFileReturn.size();i++)
                listres.append(listFileReturn[i]);
            string strFileName=listres.join("/");
            hlog(strFileName);
            string strFtpPath=strPathRemote+strNameDir+"/"+strFileName;
            hlog(strFtpPath);

            llpos=getLength(strFtpPath);
            hlog(llpos);
        }
        else if(plib::getPathType(strPathLocal)=="file")
        {
            //直接把文件名截取出来加上远程目录即可
            string strRemote=strPathRemote+lres[lres.size()-1];
            hlog(strRemote);
            llpos=getLength(strRemote);
            hlog(llpos);
        }
    }
    return 0;
}

int pftp::uploadNoReTrans(string strPathLocal, string strPathRemote, char *arrchNowFile, long long lpos)
{
    //不需要进度速度信息时，用这个原来的
    //    int iRet = Upload(strPathLocal.c_str(), strPathRemote.c_str(), this->strIp.c_str(),
    //                                 this->ptcpStatus->sock, lpos, NULL,0);
    if(!plib::isExsist(strPathLocal))
    {
        hlog(pstring()<<"文件或路径"<<strPathLocal<<"不存在，请检查本地文件或文件夹");
        return -9;
    }
    int iRet = UploadWithSpeed(strPathLocal.c_str(), strPathRemote.c_str(), this->strIp.c_str(),
                               this->ptcpStatus->sock, lpos, NULL,0,arrchNowFile);
    SendFtpCmd(this->ptcpStatus->sock, "QUIT");
    return iRet;
}

int pftp::download(string strPathLocal, string strPathRemote,long lpos)
{
    int iRet = DownloadWithSpeed(strPathLocal.c_str(), strPathRemote.c_str(), this->strIp.c_str(),
                                 this->ptcpStatus->sock, lpos, NULL);
    SendFtpCmd(this->ptcpStatus->sock, "QUIT");
    return iRet;
}


typedef struct PARA_THREAD
{
    char acPathFile[256];
    long long llSizeFile;
    long long *pllSizeSent;

}PARA_THREAD;

void thread_speed_upload(void *para)
{
    PARA_THREAD* pt=(PARA_THREAD*)para;

    while(1)
    {
        if(pt==NULL||pt->pllSizeSent==NULL)
            break;
        if(pt->llSizeFile==(*(pt->pllSizeSent)))
            break;
        //        hlog(*(pt->pllSizeSent));
        int Percent=100.0*(*(pt->pllSizeSent))/pt->llSizeFile;
        hlog(pstring()<<"文件"<<pt->acPathFile<<"的上传进度为"<<Percent<<"%");
        sleep(1);
    }

}
void thread_speed_download(void *para)
{
    PARA_THREAD* pt=(PARA_THREAD*)para;

    while(pt->llSizeFile>(*(pt->pllSizeSent)))
    {
        //        hlog(*(pt->pllSizeSent));
        int Percent=100.0*(*(pt->pllSizeSent))/pt->llSizeFile;
        hlog(pstring()<<"文件"<<pt->acPathFile<<"的下载进度为"<<Percent<<"%");
        sleep(1);
    }

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
int UploadWithSpeed(const char *pchLocalPath, const char *pchFtpPath, const char *pchIP, int iCtrlSock,long long lPos, const char *pchPostfix ,const int iPostfixFlag,char *arrchNowFile)
{
    char arrchBuf[512*1024];  //缓冲区
    char arrchFtpPath[CMD_BUF_SIZE];   //FTP路径
    char arrchLocalPath[CMD_BUF_SIZE]; //本地路径
    char arrchTmp[512*1024];       //临时缓冲区
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
            if ((iRet = UploadWithSpeed(arrchBuf, arrchFtpPath, pchIP, iCtrlSock,0, pchPostfix,iPostfixFlag,arrchNowFile)))
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

    hlog(arrchBuf);
    //这个地方的原因一般是有文件大小为0
    if ((iRet = SendFtpCmd(iCtrlSock, arrchBuf)))
    {
        HLOG("%s error:%d, %s, iRet:%d",arrchBuf,errno,strerror(errno),iRet);
        hlog(pstring()<<"请检查文件"<<pchLocalPath<<"大小是否为0");
        close(iDataSock);
        fclose(fp);
        return iRet;
    }
    HLOG("%s",arrchBuf);

    //一包长度512k
    int LEN_ONE_PACK=512*1024;
    hlog(pchLocalPath);
    hlog(llFileSize);
    PARA_THREAD pt;
    bzero(&pt,sizeof(PARA_THREAD));
    strcpy(pt.acPathFile,pchLocalPath);
    pt.llSizeFile=llFileSize;
    pt.pllSizeSent=&llSentSize;

    //统计速率另起一个线程
    pthread thAnalysis;
    thAnalysis.start(thread_speed_upload,&pt);
    ptime tstart;
    //    hlog(tstart);
    //send local file
    while (llSentSize < llFileSize)
    {
        //剩余大小大于一包，则发一包,否则发剩余大小
        if ((llRet = llFileSize - llSentSize) > LEN_ONE_PACK)
        {
            llRet = LEN_ONE_PACK;
        }
        //read file and upload
        llRet = fread(arrchBuf, sizeof(char), llRet, fp);
        //modify by lhl,如果不加超时，则因为sendfullpacket的原因会在send这卡住，不返回，无法进行断点续传等操作
        //        if ((llRet = SendFullPacket(iDataSock, arrchBuf, llRet)) <= 0)
        if ((llRet = ctcpSendFullPacketTimeout(iDataSock, arrchBuf, llRet,1)) <= 0)
        {
            hlog(pstring()<<"与ftp服务端"<<pchIP<<"连接断开");
            strcpy(arrchNowFile,pchLocalPath);
            hlog(arrchNowFile);
            //            break;
            fclose(fp);
            close(iDataSock);
            return -12;
        }
        llSentSize += llRet;

    }
    fclose(fp);
    close(iDataSock);
    ptime tend;
    //    hlog(tstart,tend);
    double dbSpeed=llFileSize*1.0/(tend-tstart)*8/1024/1024;
    hlog(pstring()<<"文件"<<pchLocalPath<<"上传使用时间为"<<tend-tstart<<"秒，上传速率为"<<dbSpeed<<"Mbps");

    //get ftp server return code
    if ((iRet = GetRetCode(iCtrlSock, 226)))
    {
        //返回500说明断开连接
        hlog(pstring()<<"与ftp服务端"<<pchIP<<"连接断开,服务端返回代码:"<<iRet);
        return -13;
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
int DownloadWithSpeed(const char *pchLocalPath, const char *pchFtpPath, const char *pchIP, int iCtrlSock, long lPos, const char *pchPostfix)
{
    PARA_THREAD pt;
    pthread thAnalysis;
    ptime tstart;
    ptime tend;

    char arrchBuf[512*1024];  //memory buffer
    char arrchFtpPath[CMD_BUF_SIZE];   //remote path
    char arrchLocalPath[CMD_BUF_SIZE]; //local path
    char arrchTmp[512*1024];       //tmp buffer
    char *pchTok;                      //tmp pointer
    int iRet;                          //return value
    int iDataSock = 0;                 //data socket
    int iItemNum;                      //LIST command info num
    int i;                             //loop varibale
    int iCWD = 0;
    long long llFileSize;                    //local file size
    long long llSentSize = 0;                //sent size
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
        hlog(pstring()<<"远程ftp目录"<<arrchFtpPath<<"不存在,请检查ftp服务目录");
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
            if ((iRet = DownloadWithSpeed(arrchLocalPath, pstruList[i].arrchFileName, pchIP, iCtrlSock, 0, pchPostfix)) < 0)
            {
                HLOG( "SendFtpCmd %s", pstruList[i].arrchFileName);
                goto DOWNLOAD_EXIT;
            }
        }

        //return to previous directory
        iRet = SendFtpCmd(iCtrlSock, "CDUP");

        iRet = 0;
        //        HLOG( "SendFtpCmd %s", pstruList[i].arrchFileName);
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
        //        if (IsDirExists(arrchTmp, "") == -1)
        //        {
        //            BuildDirectory(arrchTmp);
        //        }
        if(!plib::isExsistDir(string(arrchTmp)))
            plib::mkdirp(string(arrchTmp));
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

    llFileSize = atol(pstruList[0].arrchSize) - lPos;

    //一包长度512k
    //    int LEN_ONE_PACK=512*1024;
    hlog(arrchBuf);
    hlog(llFileSize);

    bzero(&pt,sizeof(PARA_THREAD));
    strcpy(pt.acPathFile,arrchBuf);
    pt.llSizeFile=llFileSize;
    pt.pllSizeSent=&llSentSize;

    //统计速率另起一个线程

    thAnalysis.start(thread_speed_download,&pt);
    //重新获取当前时间
    tstart.setNowTime();
    //receive all the data
    while (llSentSize < llFileSize)
    {
        iRet = RecvPacket(iDataSock, arrchTmp, sizeof(arrchTmp));
        if (iRet <= 0)
            break;
        fwrite(arrchTmp, sizeof(char), iRet, fp);
        llSentSize += iRet;
    }
    fflush(fp);
    fclose(fp);
    close(iDataSock);
    iDataSock = 0;

    tend.setNowTime();
    hlog(pstring()<<"文件"<<pchLocalPath<<"下载使用时间为"<<tend-tstart<<"秒，下载速率为"<<llFileSize*1.0/(tend-tstart)*8/1024/1024<<"Mbps");


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
    iRet = llSentSize < llFileSize ? -4 : 0;

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
