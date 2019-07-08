/*
 * 作用：实现TCP套接字的基本操作。
 * 作者：SW
 * 创建日期：2010-08-10
*/

#include "Socket.h"


/*
 * InitTcpSocket
 * 返回值：套接字，0表示初始化套接字失败
 *
 * 初始化新的Tcp套接字。
*/
int InitTcpSocket(void)
{
    int iSock = 0;

    //如果套接字初始化失败，则返回-1
    if ((iSock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        return -1;
    }
    return iSock;
}

/*
 * InitUdpSocket
 * 返回值：套接字，0表示初始化套接字失败
 *
 * 初始化新Udp套接字。
*/
int InitUdpSocket(const char *pchIPAddr, int iPort, struct sockaddr_in *pstruAddr)
{
    int iSock = 0;
    int iOn = 1;

    //如果套接字初始化失败，则返回0
    if ((iSock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        return -1;
    }

    pstruAddr->sin_family = AF_INET;
    //如果端口号是0，则使用INADDR_ANY
    pstruAddr->sin_port = iPort == 0 ? INADDR_ANY : htons(iPort);
    //如果IP地址为空，则使用INADDR_ANY
    pstruAddr->sin_addr.s_addr = pchIPAddr == NULL ? INADDR_ANY : inet_addr(pchIPAddr);

    //解决Address already in use错误
    setsockopt(iSock, SOL_SOCKET, SO_REUSEADDR, &iOn, sizeof(iOn));

    return iSock;
}

/*
 * CreateConnection
 * iSocket：  套接字
 * pchIPAddr：IP地址
 * iPort：    端口号
 * 返回值：   0表示成功
 *            其它表示失败
 *
 * 根据IP地址和端口号，创建与其的连接。
*/
int CreateConnection(int iSocket, const char *pchIPAddr, int iPort)
{
    struct sockaddr_in struSin;

    if (pchIPAddr == NULL && !strcmp("", (char *)pchIPAddr))
    {
        return -1;
    }
    if (iPort <= 0)
    {
        return -2;
    }

    //初始化套接字地址结构体
    struSin.sin_family = AF_INET;
    struSin.sin_port = htons(iPort);
    struSin.sin_addr.s_addr = inet_addr(pchIPAddr);

    //向目标地址发起TCP连接，返回状态信息
    return connect(iSocket, (struct sockaddr *)&struSin, sizeof(struSin));
}

/*
 * BindSocket
 * iSocket：  套接字
 * pchIPAddr：IP地址
 * iPort：    端口号
 *
 * 将IP地址和端口号绑定到套接字上，并开始监听TCP连接。
*/
int BindSocket(int iSocket, const char *pchIPAddr, int iPort)
{
    struct sockaddr_in struSin;

    if (BindUdpSocket(iSocket, pchIPAddr, iPort, &struSin) < 0)
    {
        return -1;
    }

    //开始监听TCP连接，如果出现错误则退出
    if (listen(iSocket, MAX_LISTEN_NUM) < 0)
    {
        return -2;
    }
    return 0;
}

/*
 * BindUdpSocket
 * iSocket：  套接字
 * pchIPAddr：IP地址
 * iPort：    端口号
 *
 * 将IP地址和端口号绑定到套接字上。
*/
int BindUdpSocket(int iSocket, const char *pchIPAddr, int iPort, struct sockaddr_in *pstruAddr)
{
    int iOn = 1;
    struct linger sopt = {1, 0};

    pstruAddr->sin_family = AF_INET;
    //如果端口号是0，则使用INADDR_ANY
    pstruAddr->sin_port = iPort == 0 ? INADDR_ANY : htons(iPort);
    //如果IP地址为空，则使用INADDR_ANY
    pstruAddr->sin_addr.s_addr = pchIPAddr == NULL ? INADDR_ANY : inet_addr(pchIPAddr);

    //解决Address already in use错误
    setsockopt(iSocket, SOL_SOCKET, SO_REUSEADDR, &iOn, sizeof(iOn));
    //setsockopt(iSocket, SOL_SOCKET, SO_REUSEPORT, &iOn, sizeof(iOn));
    setsockopt(iSocket, SOL_SOCKET, SO_LINGER, (void *)&sopt, sizeof(sopt));
    //绑定套接字到目标地址，如果出现错误则退出
    if (bind(iSocket, (struct sockaddr *)pstruAddr, sizeof(struct sockaddr_in)) < 0)
    {
        return -1;
    }
    return 0;
}

/*
 * InitSendGroupSocket
 * 返回值：套接字，0表示初始化套接字失败
 *
 * 初始化组播发送Udp套接字。
*/
int InitSendGroupSocket(const char *pchIPAddr, const char *pchGroupIPAddr, int iGroupPort, struct sockaddr_in *pstruAddr)
{
    int iSock;
    struct sockaddr_in struSin;

    if ((iSock = InitUdpSocket(pchGroupIPAddr, iGroupPort, pstruAddr)) < 0)
    {
        return -1;
    }

    memset(&struSin, 0, sizeof(struSin));
    struSin.sin_family = AF_INET;
    struSin.sin_addr.s_addr = inet_addr(pchIPAddr);

    if (bind(iSock, (struct sockaddr *)&struSin, sizeof(struSin)) < 0)
    {
        return -2;
    }
    return iSock;
}

/*
 * InitRecvGroupSocket
 * 返回值：套接字，0表示初始化套接字失败
 *
 * 初始化组播接收Udp套接字。
*/
int InitRecvGroupSocket(const char *pchIPAddr, const char *pchGroupIPAddr, int iGroupPort, struct sockaddr_in *pstruAddr)
{
    int iSock;
    struct   ip_mreq   mreq;

    if ((iSock = InitUdpSocket(NULL, iGroupPort, pstruAddr)) < 0)
    {
        return -1;
    }

    //绑定套接字到目标地址，如果出现错误则退出
    if (bind(iSock, (struct sockaddr *)pstruAddr, sizeof(struct sockaddr_in)) < 0)
    {
        return -2;
    }

    mreq.imr_multiaddr.s_addr = inet_addr(pchGroupIPAddr);
    mreq.imr_interface.s_addr = inet_addr(pchIPAddr);
    //将本机地址加入到组播
    if (setsockopt(iSock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) <0)
    {
        return   -3;
    }
    return   iSock;
}

/*
 * AcceptConnection
 * iSocket：服务端套接字
 * 返回值： 套接字
 *
 * 接受客户端TCP连接，并返回与客户端通信的套接字。
*/
int AcceptConnection(int iSocket)
{
    int iSock;
    int iLength;
    struct sockaddr_in struSin;

    iLength = sizeof(struct sockaddr_in);
    bzero(&struSin, iLength);
    //接受客户端的TCP连接，如果出现错误则退出
ACCEPT_AGAIN:
    if ((iSock = accept(iSocket, (struct sockaddr *)&struSin, (socklen_t *)&iLength)) < 0)
    {
        if (errno == EINTR)
        {
            goto ACCEPT_AGAIN;
        }
        return -1;
    }
    return iSock;
}


/*
 * CloseConnection
 * iSocket：套接字
 *
 * 关闭TCP连接套接字。
*/
void CloseConnection(int iSocket)
{
    //如果关闭套接字失败，则退出
    if ((close(iSocket)) != 0)
    {
        perror("close() failed!");
        //printf("%s\n", strerror(errno));
    }
    return;
}

/*
 * SendPacket
 * iSocket： 套接字
 * pBuffer： TCP包
 * uiLength：指定发送包的长度
 * 返回值：  实际发送包的长度
 * EBADF:   第一个参数不合法
 * EFAULT:  参数中有一指针指向无法存取的内存空间
 * ENOTSOCK:第一个参数不是socket
 * EINTR:   被信号中断
 * EAGAIN:  此操作会令进程阻塞，但socket为不可阻塞
 * ENOBUFS: 系统的缓冲区内存不足
 * ENOMEM:  核心内存不足
 * EINVAL:  传给系统调用的参数不正确
 *
 * 向对方发送TCP包。
*/
int SendPacket(int iSocket, void *pBuffer, int uiLength)
{
    //assert(pBuffer != NULL);
    //assert(uiLength > 0);

    int iRet;
    //如果发送时被信号中断，则重新进行发送
    while ((iRet = send(iSocket, pBuffer, uiLength, 0)) < 0 && errno == EINTR);
    //如果实际发送包的长度小于0，说明发送时出现了错误，等于0表示对方关闭了连接
    return iRet;
}

/*
 * RecvPacket
 * iSocket： 套接字
 * pBuffer： TCP包
 * uiLength：指定接收包的长度
 * 返回值：  实际接收包的长度
 * 特别地，返回值<0时并且(errno == EINTR || errno == EAGAIN
 *       || errno == EWOULDBLOCK)的情况下认为连接是正常的，继续接收
 *
 * 从对方接收TCP包。
*/
int RecvPacket(int iSocket, void *pBuffer, int uiLength)
{
    //assert(pBuffer != NULL);
    //assert(uiLength > 0);

    int iRet;
    //如果接收时被信号中断，则重新进行接收
    while ((iRet = recv(iSocket, pBuffer, uiLength, 0)) < 0 && errno == EINTR);
    //如果实际接收包的长度小于0，说明接收时出现了错误，等于0表示对方关闭了连接
    if(iRet<0)
    {
        printf("errno: %s\n",(strerror(errno)));
        printf("EINTR %s\n",(strerror(EINTR)));
        printf("EAGAIN %s\n",(strerror(EAGAIN)));
    }
    return iRet;
}

/*
 * SendFullPacket
 * iSocket： 套接字
 * pBuffer： TCP包
 * iLength： 指定发送包的长度
 * 返回值：  实际发送包的长度
 * 如果实际发送包的长度小于0，说明发送时出现了错误，等于0表示对方关闭了连接
 * 向对方发送特定长度的TCP包。
*/
int SendFullPacket(int iSocket, void *pBuffer, int iLength)
{
    //assert(pBuffer != NULL);
    //assert(iLength > 0);

    int iRet;
    int iSend = 0;

    while (iSend < iLength)
    {
        iRet = iLength - iSend;
        //如果发送时被信号中断，则重新进行发送
        //注释解释：如果因为资源临时不可用而长时间不返回，则导致任务长时间不能进行重连，有可能对端服务器已经断网或重启
        while ((iRet = send(iSocket, pBuffer + iSend, iRet, 0)) < 0 && (errno == EINTR /*|| errno ==EAGAIN*/))
        {
            iRet = iLength - iSend;
        }
        //如果实际发送包的长度小于0，说明发送时出现了错误，等于0表示对方关闭了连接
        if (iRet <= 0)
        {
            return iRet;
        }
        iSend += iRet;
    }
    return iSend;
}

/*
 * RecvFullPacket
 * iSocket： 套接字
 * pBuffer： TCP包
 * iLength： 指定接收包的长度
 * 返回值：  实际接收包的长度
 *
 * 从对方接收特定长度的TCP包。
*/
int RecvFullPacket(int iSocket, void *pBuffer, int iLength)
{
    //assert(pBuffer != NULL);
    //assert(iLength > 0);

    int iRet;
    int iRecv = 0;

    while (iRecv < iLength)
    {
        //update by panj20140831因为中科院大包收的太慢了
        iRet = (iLength - iRecv) > 262144 ? 262144:(iLength - iRecv);
        //iRet = iLength - iRecv;
        //如果接收时被信号中断，则重新进行接收
        //注释解释：如果因为资源临时不可用而长时间不返回，则导致任务长时间不能进行重连，有可能对端服务器已经断网或重启
        while ((iRet = recv(iSocket, pBuffer + iRecv, iRet, MSG_WAITALL)) < 0 && (errno == EINTR/* || errno ==EAGAIN*/))
        {
            //WriteLog(TRACE_NORMAL,"iRet:%d, errno:%d %d %d, %s",iRet, errno, EINTR , EAGAIN, strerror(errno));
            iRet = iLength - iRecv;
        }
        //如果实际接收包的长度小于0，说明接收时出现了错误，等于0表示对方关闭了连接
        if (iRet <= 0)
        {
            if(iRecv>0)
            {
                return iRecv;
            }
            else
            {
                return iRet;
            }
        }
        iRecv += iRet;
    }
    return iRecv;
}

/*
 *
 *	名称: icmp_cksum
 *
 *  参数: data:数据
 *				len：数据长度
 *
 *	返回值: 计算结果，short类型。
 *
 *	功能: 校验和算法，对16位的数据进行累加计算，并返回计算结果。
 *				对于奇数个字节数据的计算，是将最后的有效数据作为最高位的字节，低字节填充0。
 *
 *
*/
//校验和计算
static unsigned short icmp_cksum(unsigned char *data,int len)
{
    int sum = 0;																		//计算结果
    int odd = len & 0x01;													  //是否为奇数

    //将数据按照2字节为单位累加起来
    while(len & 0xfffe)
    {
        sum += *(unsigned short*)data;
        data += 2;
        len -= 2;
    }

    //判断是否为奇数个数据，若icmp包头为奇数个字节，会剩下最后一字节
    if(odd)
    {
        unsigned short tmp = ((*data)<<8)&0xff00;
        sum += tmp;
    }

    sum = (sum>>16) + (sum & 0xffff);				//高低位相加
    sum += (sum>>16);												//将溢出位加入
    return ~sum;														//返回取反值
}

/*
 *
 *	名称: icmp_pack
 *
 *  参数: icmph
 *				seq
 *				tv
 *				iFlag
 *	返回值:
 *
 *	功能: 设置icmp报头
 *
 *
*/
static void icmp_pack(struct icmp *icmph, int seq, int length, int iFlag)
{
    unsigned char i = 0;

    //设置报头
    icmph->icmp_type = ICMP_ECHO;					//icmp回显请求
    icmph->icmp_code =0;
    icmph->icmp_cksum = 0;
    icmph->icmp_seq = seq;								//本报序列号
    icmph->icmp_id = iFlag &0xffff;				//填写PID
    for(i=0;i<length;i++)
    {
        icmph->icmp_data[i] = i;
    }

    //计算校验和
    icmph->icmp_cksum = icmp_cksum((unsigned char*)icmph,length);
}

/*
 *
 *	名称: icmp_unpack
 *
 *  参数: buf：剥离以太网部分数据的ip数据报文
 *				len：数据长度
 *				pstruICMP_Args
 *
 *	返回值:错误信息返回-1
 *
 *	功能: 解压接收到的包
 *
 *
*/
static int icmp_unpack(char *buf,int len, char s_arrchDestStr[])
{
    int iphdrlen;

    struct ip *ip = NULL;
    struct icmp *icmp = NULL;

    ip=(struct ip *)buf;											//IP头部
    iphdrlen=ip->ip_hl*4;											//ip头部长度
    icmp=(struct icmp *)(buf+iphdrlen);				//icmp段地址
    len -= iphdrlen;

    //判断是否为icmp包
    if(len<8)
    {
        //printf("ICMP packets\'s length is less than 8\n");
        return -1;
    }

    //icmp类型为ICMP_ECHOREPL
    if((icmp->icmp_type==ICMP_ECHOREPLY) && !strcmp(s_arrchDestStr,inet_ntoa(ip->ip_src)))
    {
        //在发送表格中查找已经发送的包
        //printf("%d bytes from %s: icmp_seq=%u ttl=%d\n", len, inet_ntoa(ip->ip_src), icmp->icmp_seq, ip->ip_ttl);
        return icmp->icmp_seq;
    }
    return -1;
}

/*
 *
 *	名称: CheckNetwork
 *
 *  参数: buf：剥离以太网部分数据的ip数据报文
 *
 *	返回值:正常返回0，不正常返回-1
 *
 *	功能: 用icmp协议检查网络是否正常
 *
*/
int CheckNetwork(char *pchIpAddr)
{
    int i;
    struct protoent *protocol = NULL;
    struct sockaddr_in dest;
    char arrchBuf[512];
    int iRet;
    int iTimes = 0;
    int iRawSock;
    struct timeval tmUp;                         //超时时间

    //获取协议类型ICMP
    if((protocol = getprotobyname("icmp")) == NULL)
    {
        perror("getprotobyname()");
        return -1;
    }

    if (-1 == (iRawSock = socket(AF_INET, SOCK_RAW, protocol->p_proto)))
    {
        return -2;
    }
    dest.sin_family = AF_INET;
    dest.sin_addr.s_addr = inet_addr(pchIpAddr);

    //设置超时时间
    tmUp.tv_sec = 5;
    tmUp.tv_usec = 0;
    if (setsockopt(iRawSock, SOL_SOCKET, SO_RCVTIMEO, (char *)&tmUp, sizeof(tmUp)) == -1)
    {
        close(iRawSock);
        return -1;
    }

    //Check 4 times
    for (i=0; i<4; i++)
    {
        memset(arrchBuf, 0, sizeof(arrchBuf));
        icmp_pack((struct icmp *)arrchBuf, i, 64, getpid());
        if (-1 == sendto(iRawSock, arrchBuf, 64, 0, (struct sockaddr *)&dest, sizeof(dest)))
        {
            close(iRawSock);
            return -3;
        }
        //接收数据
        if (-1 == (iRet = recv(iRawSock, arrchBuf, sizeof(arrchBuf), 0)))
        {
            sleep(1);
            continue;
        }
        //解包
        if (0 <= icmp_unpack(arrchBuf, iRet, pchIpAddr))
        {
            iTimes++;
        }
        sleep(1);
    }

    close(iRawSock);
    return iTimes > 0 ? 0 : -1;
}

/*
 * RecvFullPacket
 * iSocket： 套接字
 * pBuffer： TCP包
 * iLength： 指定接收包的长度
 * iTimes:   重试次数
 * 返回值：  正常：实际接收包的长度
 *          断网: -3
 *          超时: -2
 *
 * 从对方接收特定长度的TCP包，并检测接收的状态
*/
int AutoRecvPacket(int iSocket, void *pBuffer, int iLength, char *pchIP, int iTimes)
{
    int iRet;
    int iRecv = 0;
    int iRecvTimes = 0;

    if (3 > iSocket || NULL == pBuffer || 1 > iLength)
        return -1;

    if (0 > iTimes)
        iTimes = 0;

    while (iRecv < iLength)
    {
        //如果接收时被信号中断，则重新进行接收
        iRet = recv(iSocket, pBuffer + iRecv, iLength - iRecv, MSG_WAITALL);
        //如果实际接收包的长度小于0，说明接收时出现了错误，等于0表示对方关闭了连接
        if (0 < iRet)
        {
            iRecv += iRet;
            continue;
        }
        //如果返回0，表示对方关闭连接
        else if (0 == iRet)
        {
            break;
        }
        //如果被信号中断，则继续接收
        else if (-1 == iRet && errno == EINTR)
        {
            continue;
        }
        //判断是否超过重试次数
        if (iRecvTimes++ == iTimes)
        {
            return -2;
        }
        //检测是否断网
        if (NULL != pBuffer && strcmp("", (char *)pBuffer)!=0)
        {
            if (-1 == CheckNetwork(pchIP))
            {
                return -3;
            }
        }
    }
    return iRecv;
}

/*
 * AutoSendPacket
 * iSocket： 套接字
 * pBuffer： TCP包
 * iLength： 指定Send包的长度
 * iTimes:   重试次数
 * 返回值：  正常：实际Send的长度
 *          断网: -3
 *          超时: -2
 *
 * Send specify packet and check the sending status
*/
int AutoSendPacket(int iSocket, void *pBuffer, int iLength, char *pchIP, int iTimes)
{
    int iRet;
    int iSend = 0;
    int iSendTimes = 0;

    if (3 > iSocket || NULL == pBuffer || 1 > iLength)
        return -1;

    if (0 > iTimes)
        iTimes = 0;

    while (iSend < iLength)
    {
        iRet = send(iSocket, pBuffer + iSend, iLength - iSend, 0);
        //如果实际发送包的长度小于0，说明发送时出现了错误，等于0表示对方关闭了连接
        if (0 < iRet)
        {
            iSend += iRet;
            continue;
        }
        //如果返回0，表示对方关闭连接
        else if (0 == iRet)
        {
            break;
        }
        //如果被信号中断，则继续接收
        else if (-1 == iRet && errno == EINTR)
        {
            continue;
        }
        else if (-1 == iRet && (errno == ECONNRESET || errno == EACCES || errno == EPIPE))
        {
            break;
        }
        //判断是否超过重试次数
        if (iSendTimes++ == iTimes)
        {
            return -2;
        }
        //检测是否断网
        if (NULL != pBuffer &&strcmp("", (char *)pBuffer)!=0 )
        {
            if (-1 == CheckNetwork(pchIP))
            {
                return -3;
            }
        }
    }
    return iSend;
}


/*
 * KeepSockAlive
 * 参数:iSock，要设置的socket
 * 参数:iAlvTm，
 * 参数:iIdleTm，开始首次KeepAlive探测前的TCP空闲时间
 * 参数:iIntTm，两次KeepAlive探测间的时间间隔
 * 参数:iCnt，判定断开前的KeepAlive探测次数
 * 返回值：int
 *
 * 函数说明：TCP连接保活机制设置
 * add by panj 20150724
*/
int KeepSockAlive(int iSock, int iAlvTm, int iIdleTm, int iIntTm, int iCnt)
{
    assert(iSock > 2);

    int iRet;
    if (!(iRet = setsockopt(iSock, SOL_SOCKET, SO_KEEPALIVE, (void *)&iAlvTm, sizeof(iAlvTm))))
    {
        return iRet;
    }
    if (!(iRet = setsockopt(iSock, SOL_TCP, TCP_KEEPIDLE, (void *)&iIdleTm, sizeof(iIdleTm))))
    {
        return iRet;
    }
    if (!(iRet = setsockopt(iSock, SOL_TCP, TCP_KEEPINTVL, (void *)&iIntTm, sizeof(iIntTm))))
    {
        return iRet;
    }
    if (!(iRet = setsockopt(iSock, SOL_TCP, TCP_KEEPCNT, (void *)&iCnt, sizeof(iCnt))))
    {
        return iRet;
    }

    return 1;
}
