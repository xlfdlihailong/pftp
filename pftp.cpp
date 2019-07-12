#include "pftp.h"

pftp::pftp()
{
    this->strRealIp="";
    this->iPortPasv=-1;
}

//状态连接最后关,数据连接要即使关
pftp::~pftp()
{
    close(sockStatus);
}

int pftp::connect(string ip, string user, string pwd, int port)
{
    this->strIp=ip;
    this->strUser=user;
    this->strPwd=pwd;
    this->iPort=port;

    this->sockStatus = socket(AF_INET, SOCK_STREAM, 0);
    if (ConnectFtp(sockStatus, ip.c_str(), port, user.c_str(), pwd.c_str()) < 0)
    {
        hlog(pstring()<<"connect"<<ip<<"fail");
        close(sockStatus);
        return -1;
    }

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
    return SendFtpCmd(this->sockStatus,cmd.c_str());
}
//返回空表示失败
string pftp::pwd()
{
    char arrchRecv[256];
    bzero(arrchRecv,256);
    int iRes=SendFtpCmdWithRecv(this->sockStatus,"PWD ",arrchRecv);
    //    hlog(iRes);
    if(iRes<0)
        return "";
//    hlog(arrchRecv);
    return string(arrchRecv);
}

int pftp::cd(string strdes)
{
    string strcmd="CWD "+strdes;
    hlog(strcmd);
    return SendFtpCmd(this->sockStatus,strcmd.c_str());
}

int pftp::quit()
{
    return sendcmd("QUIT");
}

long long pftp::getLength(string path)
{
    string cmd="SIZE "+path+"\r\n";
    //    hlog(cmd);
    int iSendRes= SendPacket(this->sockStatus, (void*)cmd.c_str(), cmd.size());
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
    return RecvPacket(this->sockStatus,dataRecv,len);
}

int pftp::isExsistDir(string path)
{
    FTP_LIST_ITEM_STRUCT *pstruList = NULL;
    int iItemNum=-9;
    if ((iItemNum=ParseFtpListCmd(this->sockStatus, path.c_str(), this->strIp.c_str(), &pstruList)) <= 0)
    {
        HLOG( "ParseFtpListCmd %d, %s", iItemNum, path.c_str());
        hlog(pstring()<<"远程ftp目录"<<path<<"不存在,请检查ftp服务目录");
        return -1; //test 20131104
    }
    HLOG( "ParseFtpListCmd %d, %s", iItemNum, path.c_str());
    return 0;
}

//被动模式，同时获取真实ip和端口，要在能访问的地方连接虚拟ip，否则不返回真实ip
int pftp::setPASV()
{
    int iRet=-99;
    char arrchTmp[256];
    memset(arrchTmp,0,sizeof(arrchTmp));

    //这个返回的iRet改为端口号了
    if ((iRet = SendPASVFtpCmd(this->sockStatus, "PASV",arrchTmp)) < 0)
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

int pftp::upload(string strPathLocal, string strPathRemote)
{
    if(!plib::isExsist(strPathLocal))
    {
        hlog(pstring()<<"文件或路径"<<strPathLocal<<"不存在，请检查本地文件或文件夹");
        return -9;
    }
    //    int iRet = UploadWithSpeedUpdateByXlfd(strPathLocal.c_str(), strPathRemote.c_str(), this->strIp.c_str(),
    //                               this->iPort,this->strUser.c_str(),this->strPwd.c_str(), this->sockStatus, 0, NULL,0);


    int iRet = UploadWithSpeedUpdateByXlfdInner(strPathLocal.c_str(), strPathRemote.c_str(),NULL,0);
    return iRet;
}


int pftp::download(string strPathLocal, string strPathRemote,long lpos)
{
    //    int iRet = DownloadWithSpeed(strPathLocal.c_str(), strPathRemote.c_str(), this->strIp.c_str(),
    //                                 this->sockStatus, lpos, NULL);
    //    SendFtpCmd(this->sockStatus, "QUIT");

    if(!plib::isExsist(strPathLocal))
    {
        hlog(pstring()<<"文件或路径"<<strPathLocal<<"不存在，请检查本地文件或文件夹");
        return -9;
    }
    int iRet = DownloadWithSpeedUpdateByXlfdInner(strPathLocal.c_str(), strPathRemote.c_str(),NULL,0);
    return iRet;
}



typedef struct PARA_THREAD
{
    char acPathFile[256];
    long long llSizeFile;
    long long *pllSizeSent;
    int exit;
}PARA_THREAD;

void thread_speed_trans(void *para)
{
    pthread_detach(pthread_self());
    PARA_THREAD* pt=(PARA_THREAD*)para;
    char arrchFile[256];
    bzero(arrchFile,256);
    strcpy(arrchFile,pt->acPathFile);
    while(!pt->exit)
    {
        pthread_testcancel();
        if(pt==NULL||pt->pllSizeSent==NULL)
            break;
        long long llsent=(*(pt->pllSizeSent));
        if(pt->llSizeFile==llsent)
            break;
        //        hlog(*(pt->pllSizeSent));
        int Percent=100.0*llsent/pt->llSizeFile;
        if(Percent<0||Percent>100)
            return;
        hlog(pstring()<<"文件"<<pt->acPathFile<<"的传输进度为"<<Percent<<"%");
        sleep(1);
    }
    hlog(pstring()<<"文件"<<arrchFile<<"的统计速率线程退出");
}

int pftp::UploadWithSpeedUpdateByXlfdInner(const char *pchLocalPath, const char *pchFtpPath, const char *pchPostfix, const int iPostfixFlag)
{
    long long lPos=0;
    pthread thAnalysis;
    PARA_THREAD pt;
retrans:
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
    if (pchFtpPath == NULL || lPos < 0 || this->sockStatus < 2 ||
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
            if ((iRet = UploadWithSpeedUpdateByXlfdInner(arrchBuf, arrchFtpPath, pchPostfix,iPostfixFlag)))
            {
                closedir(pDp);
                return iRet;
            }
        }
        closedir(pDp);
        return 0;
    }



    //    hlog(arrchFtpPath);
    //upload regular file
    if (strcmp(arrchFtpPath, "") && strcmp(arrchFtpPath, "."))
    {
        strcat(arrchFtpPath, "/");
    }

    //deal ftp path like "", "." or "dir_name"
    pchTok = strrchr(arrchLocalPath, '/');
    strcat(arrchFtpPath, pchTok != NULL ? pchTok + 1 : arrchLocalPath);
    //    hlog(arrchFtpPath);
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
        if (SendFtpCmd(this->sockStatus, arrchTmp))
        {
            //create ftp dir
            //先判断是否有该文件夹，如果有就不用创建了


            sprintf(arrchTmp, "MKD %s", pchTok);
            if ((iRet = SendFtpCmd(this->sockStatus, arrchTmp)))
            {
                hlog(iRet,arrchTmp);
                return iRet;
            }

            //enter ftp dir
            sprintf(arrchTmp, "CWD %s", pchTok);
            hlog(arrchTmp);
            if ((iRet = SendFtpCmd(this->sockStatus, arrchTmp)))
            {
                hlog(iRet);
                return iRet;
            }
        }
        pchStr = NULL;
        iLevel++; //ftp dir path depth
    }


    //被动模式连接ftp服务，如果是虚拟ip，则返回真实ip和端口
    //connect ftp server using PASV mode
    memset(arrchTmp,0,sizeof(arrchTmp));
    if ((iRet = SendPASVFtpCmd(this->sockStatus, "PASV",arrchTmp)) < 0)
    {
        return iRet;
    }
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
    if ((iRet = SendFtpCmd(this->sockStatus, "TYPE I")))
    {
        HLOG("TYPE I error:%d, %s",errno,strerror(errno));
        close(iDataSock);
        return iRet;
    }
    //    HLOG("TYPE I");
    //open local file and get its size
    if ((fp = fopen(pchLocalPath, "r")) == NULL)
    {
        hlog(pstring()<<"打开文件"<<pchLocalPath<<"失败");
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
    //    hlog(lPos);
    //    hlog(arrchBuf);
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
    //    hlog(arrchBuf);
    //这个地方的原因一般是有文件大小为0
    if ((iRet = SendFtpCmd(this->sockStatus, arrchBuf)))
    {
        HLOG("%s error:%d, %s, iRet:%d",arrchBuf,errno,strerror(errno),iRet);
        hlog(pstring()<<"请检查文件"<<pchLocalPath<<"大小是否为0");
        close(iDataSock);
        fclose(fp);
        return iRet;
    }
    //    HLOG("%s",arrchBuf);
    //一包长度512k
    int LEN_ONE_PACK=512*1024;
    //    hlog(pchLocalPath);


    hlog(llFileSize);
    bzero(&pt,sizeof(PARA_THREAD));
    strcpy(pt.acPathFile,pchLocalPath);
    pt.llSizeFile=llFileSize;
    pt.pllSizeSent=&llSentSize;
    pt.exit=0;
    //统计速率另起一个线程

    thAnalysis.start(thread_speed_trans,&pt);
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
        //         hlog(pchFtpPath,pchFtpFileName,pchLocalPath,this->pwd());
        if ((llRet = ctcpSendFullPacketTimeout(iDataSock, arrchBuf, llRet,1)) <= 0)
        {
            fclose(fp);
            close(iDataSock);
            pt.exit=1;
            thAnalysis.kill();
            hlog(pstring()<<"与ftp服务端"<<this->strIp<<"连接断开");
            hlog(this->reconnect());


            //直接把文件名截取出来加上远程目录即可
            string strPathRemote=string(pchFtpPath);
            if(strPathRemote[strPathRemote.size()-1]!='/')
                strPathRemote=strPathRemote+"/";

            pliststring lres=pstring(pchLocalPath).split("/");
            string strRemote=strPathRemote+lres[lres.size()-1];
            hlog(strRemote);
            lPos=this->getLength(strRemote);
            hlog(lPos);


            goto retrans;
        }
        llSentSize += llRet;
        //        int percent=100.0*llSentSize/llFileSize;
        //        hlog(pstring()<<"文件"<<pchLocalPath<<"的传输百分比为"<<percent<<"%");
    }
    fclose(fp);
    close(iDataSock);
    ptime tend;
    //    hlog(tstart,tend);
    double dbSpeed=llFileSize*1.0/(tend-tstart)*8/1024/1024;
    hlog(pstring()<<"文件"<<pchLocalPath<<"上传使用时间为"<<tend-tstart<<"秒，上传速率为"<<dbSpeed<<"Mbps");

    //get ftp server return code
    if ((iRet = GetRetCode(this->sockStatus, 226)))
    {
        //返回500说明断开连接
        hlog(pstring()<<"与ftp服务端"<<this->strIp<<"连接断开,服务端返回代码:"<<iRet);
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
        if ((iRet = SendFtpCmd(this->sockStatus, arrchBuf)))
        {
            return iRet;
        }

        memset(arrchBuf, 0, sizeof(arrchBuf));
        sprintf(arrchBuf, "RNTO %s", pchFtpFileName);
        if ((iRet = SendFtpCmd(this->sockStatus, arrchBuf)))
        {
            return iRet;
        }
    }

    //change to previous ftp directory
    for (i=0; i<iLevel; i++)
    {
        iRet = SendFtpCmd(this->sockStatus, "CDUP");
    }

    //    hlog(llSentSize,llFileSize);
    //send failed if filesize greater than zero
    if (llSentSize < llFileSize)
    {
        return -5;
    }
    pt.exit=1;
    thAnalysis.kill();
    return 0;
}

int pftp::DownloadWithSpeedUpdateByXlfdInner(const char *pchLocalPath, const char *pchFtpPath, const char *pchPostfix, const int iPostfixFlag)
{
    long long lPos=0;
    PARA_THREAD pt;
    pthread thAnalysis;
    ptime tstart;
    ptime tend;
    string strpwd="";
redownload:
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
    if (pchFtpPath == NULL ||  pchLocalPath == NULL || lPos < 0)
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
    hlog(arrchFtpPath);
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


    strpwd=(this->pwd());
    hlog(strpwd);
    //get information about ftp path
    if ((iItemNum = ParseFtpListCmd(this->sockStatus, arrchFtpPath, this->strIp.c_str(), &pstruList)) <= 0)
    {
        HLOG( "ParseFtpListCmd %d, %s", iItemNum, arrchFtpPath);
        hlog(pstring()<<"远程ftp目录"<<arrchFtpPath<<"查询失败,itemNum:"<<iItemNum);

        //        close(this->sockStatus);
        //        this->reconnect();
        return -9; //test 20131104
    }
    HLOG( "ParseFtpListCmd %d, %s", iItemNum, arrchFtpPath);

    pchTok = strrchr(arrchFtpPath, '/');
    pchTok = pchTok == NULL ? arrchFtpPath : pchTok + 1;

    //if ftp path is directory, enter the dir.
    if (iItemNum > 1 || pstruList[0].arrchAttributes[0] == 'd'
            || strcmp(pchTok, pstruList[0].arrchFileName))
    {
        //        hlog("is dir");
        hlog(arrchFtpPath);
        //enter ftp dir
        memset(arrchTmp, 0, sizeof(arrchTmp));
        sprintf(arrchTmp, "CWD %s", arrchFtpPath);
        hlog(arrchTmp);
        if ((iRet = SendFtpCmd(this->sockStatus, arrchTmp)))
        {
            HLOG( "SendFtpCmd %s", arrchTmp);
            goto DOWNLOAD_EXIT;
        }

        //download all files and folders
        for (i=0; i<iItemNum; i++)
        {
            if ((iRet = DownloadWithSpeedUpdateByXlfdInner(arrchLocalPath, pstruList[i].arrchFileName,  pchPostfix,iPostfixFlag)) < 0)
            {
                HLOG( "SendFtpCmd %s", pstruList[i].arrchFileName);
                goto DOWNLOAD_EXIT;
            }
        }
        //return to previous directory
        iRet = SendFtpCmd(this->sockStatus, "CDUP");
        iRet = 0;
        //        HLOG( "SendFtpCmd %s", pstruList[i].arrchFileName);
        goto DOWNLOAD_EXIT;
    }
    else
    {
        //        hlog("is file");

        pchTok = strrchr(arrchFtpPath, '/');
        hlog(arrchFtpPath);
        if (pchTok != NULL)
        {
            //enter ftp dir
            memset(arrchTmp, 0, sizeof(arrchTmp));
            memcpy(arrchTmp, arrchFtpPath, pchTok - arrchFtpPath);
            memset(arrchBuf, 0, sizeof(arrchBuf));
            sprintf(arrchBuf, "CWD %s", arrchTmp);
            if ((iRet = SendFtpCmd(this->sockStatus, arrchBuf)))
            {
                HLOG( "SendFtpCmd %s", arrchBuf);
                goto DOWNLOAD_EXIT;
            }
            iCWD = 1;
        }
    }


    //connect ftp server using PASV mode
    memset(arrchTmp, 0, sizeof(arrchTmp));
    if ((iRet = SendPASVFtpCmd(this->sockStatus, "PASV",arrchTmp)) < 0)
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
    if ((iRet = SendFtpCmd(this->sockStatus, "TYPE I")) < 0)
    {
        HLOG( "SendFtpCmd %d", iRet);
        goto DOWNLOAD_EXIT;
    }

    //support REST command
    if (lPos > 0)
    {
        sprintf(arrchBuf, "REST %ld", lPos);
        if ((iRet = SendFtpCmd(this->sockStatus, arrchBuf)) < 0)
        {
            HLOG( "SendFtpCmd %s", arrchBuf);
            goto DOWNLOAD_EXIT;
        }
    }

    //send RETR command to ftp server
    sprintf(arrchBuf, "RETR %s", pstruList[0].arrchFileName);
    if ((iRet = SendFtpCmd(this->sockStatus, arrchBuf)) < 0)
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

    //此处要改成与upload的一样，如果lpos是0，则要覆盖，否则追加，不然一直追加
    if(!lPos)
    {
        //open local file for write
        if ((fp = fopen(arrchBuf, "wb+")) == NULL)
        {
            iRet = -3;
            fclose(fp); //add 20131122
            goto DOWNLOAD_EXIT;
        }
    }
    else
    {
        //open local file for write
        if ((fp = fopen(arrchBuf, "ab+")) == NULL)
        {
            iRet = -3;
            fclose(fp); //add 20131122
            goto DOWNLOAD_EXIT;
        }
    }

    hlog(arrchBuf);
    llFileSize = atol(pstruList[0].arrchSize) - lPos;

    //一包长度512k
    //    int LEN_ONE_PACK=512*1024;
    //    hlog(arrchBuf);
    hlog(llFileSize);

    bzero(&pt,sizeof(PARA_THREAD));
    strcpy(pt.acPathFile,arrchBuf);
    pt.llSizeFile=llFileSize;
    pt.pllSizeSent=&llSentSize;

    //统计速率另起一个线程
    //    hlog(this->pwd());
    thAnalysis.start(thread_speed_trans,&pt);
    //重新获取当前时间
    tstart.setNowTime();
    //receive all the data
    while (llSentSize < llFileSize)
    {
        iRet = RecvPacket(iDataSock, arrchTmp, sizeof(arrchTmp));
        if (iRet <= 0)
        {

            hlog(iRet);
            fflush(fp);
            fclose(fp);
            close(iDataSock);
            close(this->sockStatus);
            pt.exit=1;
            //            hlog(thAnalysis.kill());
            hlog(pstring()<<"与ftp服务端"<<this->strIp<<"连接断开");
            hlog(strpwd);
            pliststring listpwd=pstring(strpwd).split("/");
//            hlog(listpwd);
            hlog(this->reconnect());
            string strpwdDefault=(this->pwd());
//            hlog(strpwdDefault);
            pliststring listpwdDefault=pstring(strpwdDefault).split("/");
            pliststring listdes;
            for(int i=listpwdDefault.size();i<listpwd.size();i++)
                listdes.append(listpwd[i]);
            //                        hlog(listdes);
            string strdes=listdes.join("/");
            hlog(strdes);

            hlog(this->pwd());
            hlog(this->cd(strdes));
            hlog(this->pwd());
            //            strpwd=strdes;

            //            //获取本地文件大小
            lPos=plib::getFileSize(string(arrchBuf));
            hlog(lPos);
            goto redownload;
        }
        fwrite(arrchTmp, sizeof(char), iRet, fp);
        llSentSize += iRet;

        //        int percent=100.0*llSentSize/llFileSize;
        //        hlog(pstring()<<"文件"<<arrchBuf<<"的传输百分比为"<<percent<<"%");
    }
    fflush(fp);
    fclose(fp);
    close(iDataSock);
    iDataSock = 0;
    pt.exit=1;
    //    thAnalysis.kill();

    tend.setNowTime();
    hlog(tstart,tend);
    hlog(pstring()<<"文件"<<arrchBuf<<"下载使用时间为"<<tend-tstart<<"秒，下载速率为"<<llFileSize*1.0/(tend-tstart)*8/1024/1024<<"Mbps");


    //get ftp server return code
    if ((iRet = GetRetCode(this->sockStatus, 226)))
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
            iRet = SendFtpCmd(this->sockStatus, "CDUP");
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
int SendFtpCmdWithRecv(int iSock, const char *pchCmd,char* arrchRecv)
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

#ifdef DEBUG
    HLOG( "%s", arrchBuf);
#endif

    if (0 >= SendPacket(iSock, arrchBuf, strlen(arrchBuf)))
    {
        return -3;
    }
    //接收FTP服务器的返回代码，如果接收失败，返回错误代码
    //HLOG( "%s", pchCmd);
    iRet = GetRetCodeWithRecv(iSock, iRet,arrchRecv);
    if (!strncmp(pchCmd, "PASV", 4))
    {
        HLOG( "%s", pchCmd);
        return iRet;
    }
    //    hlog(arrchRecv);
    return 0;
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
int GetRetCodeWithRecv(int iSock, int iCorrectCode,char* arrchRecv)
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
            HLOG("RecvPacket error , %d", iRet);
            return -2;
        }
        else if (iRet == 0)
            break;
        iRecvSize += iRet;
        if(iRecvSize>=CMD_BUF_SIZE)
        {
            HLOG("iRecvSize = %d", iRecvSize);
            return -2;
        }
    }
    while (arrchBuf[iRecvSize - 1] != '\n'); //use '\n' as end mark

    arrchBuf[iRecvSize] = 0;
    //    HLOG("%s",arrchBuf);

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
    //    hlog(arrchBuf);
    //get result buf
    pstring strbuf=pstring(arrchBuf);
    //    hlog(strbuf);
    //把分号也去掉
    pliststring listbuf=strbuf.split(" \r\n\"");
    //        hlog(listbuf);
    strcpy(arrchRecv,listbuf[1].c_str());

    return iRet == iCorrectCode ? 0 : iRet;
}
