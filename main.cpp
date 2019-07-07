#include "Ftp.h"
#include "../plib/plib.h"
#include "pftp.h"
int main()
{
    //    WriteLog(TRACE_NORMAL,"Start upload %s ",arrchDescFileName);
    //    hlog(UploadConnBindSrcAddr(arrchDescFileName, arrchRemotePath, pstruDestInfo->arrchIPRemoteAddr,
    //                               pstruDestInfo->iFtpPortRemote,
    //                               pstruDestInfo->arrchFtpUser,
    //                               pstruDestInfo->arrchFtpPwd, 0, NULL, 0, arrchSrcIP));
//    hlog(UploadConnBindSrcAddr("/root/send.dat","xlfd","106.13.71.127",21,"sjcs","sjcsfwq",0,NULL,0,"192.168.211.153"));
//    HLOG("xxxxx");

    pftp ftp;
    hlog(ftp.connect("106.13.71.127","sjcs","sjcsfwq"));
//    hlog(ftp.connect("172.16.14.3","sjcs","sjcs2014"));

//    hlog(ftp.setPASV());
//    hlog(ftp.upload("/root/xinstall/dat2","dadiaofei"));
    hlog(ftp.upload("/root/xinstall/","dadiaofei"));
//    hlog(ftp.upload("/mnt/snfs/50Gb.file","dadiaofei"));

    hlog(ftp.quit());
    return 0;
}

