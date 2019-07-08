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
int main()
{
    //不加这句send会挂
    plib::setSignals(sigExit,sigPipe);
    pftp ftp;
    if(ftp.connect("106.13.71.127","sjcs","sjcsfwq")<0)
    {
        ftp.reconnect();
    }

    hlog(ftp.upload("/root/data","dadiaofei"));
    hlog(ftp.upload("/root/xupload","dadiaofei"));
    hlog(ftp.download("/root","dadiaofei/xinstall/dat1"));
    hlog(ftp.download("/root","dadiaofei/xinstall"));

    hlog(ftp.quit());
    return 0;
}

