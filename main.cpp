#include "Ftp.h"
#include "../plib/plib.h"
#include "pftp.h"
void sigExit(int id)
{

}
void sigPipe(int id)
{

}


//测试两遍，一遍是正常的，四个上传文件，上传文件夹，下载文件，下载文件夹
//一遍是断点续传，再每个文件传输过程中都杀掉vsftpd服务测试
void testTrans(pftp& ftp)
{
    hlog(ftp.upload("/root/data","dadiaofei"));
    hlog(ftp.upload("/root/xupload","dadiaofei"));
    hlog(ftp.download("/root","dadiaofei/xinstall/dat1"));
    hlog(ftp.download("/root","dadiaofei/xinstall"));
}

//######################未解决########################
//获取真实ip
void testGetRealIP(pftp& ftp)
{
//    hlog(ftp.pwd());
    ftp.setPASV();
}

int main()
{
    string host,user,pwd;
    host="172.16.141.11";
    user="sjcs";
    pwd="sjcs2014";
    //不加这句send会挂
    plib::setSignals(sigExit,sigPipe);
    pftp ftp;
    if(ftp.connect(host,user,pwd)<0)
    {
        hlog(pstring()<<"连接ftp服务"<<host<<"失败,重连中");
        ftp.reconnect();
    }

    testGetRealIP(ftp);
//    testTrans(ftp);

    hlog(ftp.quit());
    return 0;
}

