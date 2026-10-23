#include "map_header.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/process.h>
#include <cell/cell_fs.h>
#include <cell/sysutil.h>
#include <cell/msg_dialog.h>
SYS_PROCESS_PARAM(1001,0x100000);
static uint8_t header[H3_MAP_HEADER_SIZE],modified[H3_MAP_HEADER_SIZE];
static volatile int done;
static void button(int value,void *ctx){(void)value;(void)ctx;done=1;}
static void event(uint64_t status,uint64_t param,void *ctx){
    (void)param;(void)ctx;if(status==CELL_SYSUTIL_REQUEST_EXITGAME)done=1;
}
static unsigned checks(uint64_t size) {
    struct h3_map_header out;unsigned passed=0;
    passed+=h3_map_parse(header,sizeof(header),size,&out)==H3_MAP_OK;
    passed+=h3_map_parse(header,sizeof(header)-1,size,&out)==H3_MAP_SHORT_HEADER;
    passed+=h3_map_parse(NULL,sizeof(header),size,&out)==H3_MAP_SHORT_HEADER;
    memcpy(modified,header,sizeof(header));modified[0]^=1;
    passed+=h3_map_parse(modified,sizeof(modified),size,&out)==H3_MAP_BAD_HEAD;
    memcpy(modified,header,sizeof(header));modified[7]=12;
    passed+=h3_map_parse(modified,sizeof(modified),size,&out)==H3_MAP_BAD_VERSION;
    passed+=h3_map_parse(header,sizeof(header),size+1,&out)==H3_MAP_BAD_SIZE;
    memcpy(modified,header,sizeof(header));modified[0x2ffc]^=1;
    passed+=h3_map_parse(modified,sizeof(modified),size,&out)==H3_MAP_BAD_FOOT;
    memcpy(modified,header,sizeof(header));memset(modified+0x18c,'X',32);
    passed+=h3_map_parse(modified,sizeof(modified),size,&out)==H3_MAP_BAD_STRING;
    return passed;
}
static void save_result(const char *text) {
    int fd;uint64_t written=0;
    int rc=cellFsOpen("/app_home/map-reader-result.txt",CELL_FS_O_CREAT|CELL_FS_O_TRUNC|CELL_FS_O_WRONLY,&fd,NULL,0);
    if(rc==0){rc=cellFsWrite(fd,text,strlen(text),&written);cellFsClose(fd);}
    printf("H3MAP_REPORT_WRITE rc=%08x bytes=%llu\n",(unsigned)rc,(unsigned long long)written);
}
int main(void) {
    int fd=-1,rc;uint64_t bytes=0,total=0;CellFsStat st;
    struct h3_map_header parsed;char report[1536],message[768];
    int success=0;unsigned passed=0;
    const char *path="/app_home/mainmenu.map";
    printf("H3MAP_BEGIN path=%s\n",path);
    rc=cellFsOpen(path,CELL_FS_O_RDONLY,&fd,NULL,0);
    if(rc!=0){snprintf(report,sizeof(report),"FAIL open %s rc=%08x\n",path,(unsigned)rc);goto display;}
    rc=cellFsFstat(fd,&st);
    if(rc!=0){snprintf(report,sizeof(report),"FAIL fstat rc=%08x\n",(unsigned)rc);goto close_file;}
    while(total<sizeof(header)) {
        rc=cellFsRead(fd,header+total,sizeof(header)-total,&bytes);
        if(rc!=0||bytes==0)break;
        total+=bytes;
    }
    if(rc!=0){snprintf(report,sizeof(report),"FAIL read rc=%08x bytes=%llu\n",(unsigned)rc,(unsigned long long)total);goto close_file;}
    enum h3_map_status status=h3_map_parse(header,(size_t)total,st.st_size,&parsed);
    if(status!=H3_MAP_OK){snprintf(report,sizeof(report),"FAIL parse: %s\n",h3_map_status_text(status));goto close_file;}
    passed=checks(st.st_size);success=passed==8;
    snprintf(report,sizeof(report),
        "H3MAP_%s\npath=%s\nname=%s\nversion=%u\nfile_size=%llu\nheader_bytes=%llu\nbuild=%s\nscenario=%s\nheader_crc32=%08x\nparser_checks=%u/8\n",
        success?"PASS":"FAIL",path,parsed.name,(unsigned)parsed.version,(unsigned long long)st.st_size,
        (unsigned long long)total,parsed.build,parsed.scenario,(unsigned)parsed.header_crc32,passed);
close_file:
    rc=cellFsClose(fd);
    if(rc!=0){success=0;snprintf(report,sizeof(report),"FAIL close rc=%08x\n",(unsigned)rc);}
display:
    fputs(report,stdout);fflush(stdout);save_result(report);
    if(success)snprintf(message,sizeof(message),
        "H3-PS3: MAP HEADER OK\n%s | version %u | %u bytes\nBuild: %s\nScenario: %s\nHeader CRC32: %08x | checks: %u/8\nSolo cabecera; no se ha cargado el nivel. Cierre automatico en 25 s.",
        parsed.name,(unsigned)parsed.version,(unsigned)parsed.declared_size,parsed.build,parsed.scenario,(unsigned)parsed.header_crc32,passed);
    else snprintf(message,sizeof(message),"H3-PS3: ERROR\n%.700s",report);
    rc=cellSysutilRegisterCallback(0,event,NULL);
    if(rc!=0){printf("H3MAP_CALLBACK_FAIL %08x\n",(unsigned)rc);return 4;}
    unsigned type=CELL_MSGDIALOG_TYPE_SE_TYPE_NORMAL|CELL_MSGDIALOG_TYPE_BG_VISIBLE|
        CELL_MSGDIALOG_TYPE_BUTTON_TYPE_YESNO|CELL_MSGDIALOG_TYPE_DEFAULT_CURSOR_YES;
    rc=cellMsgDialogOpen2(type,message,button,NULL,NULL);
    if(rc!=0){printf("H3MAP_DIALOG_FAIL %08x\n",(unsigned)rc);cellSysutilUnregisterCallback(0);return 5;}
    puts("H3MAP_DIALOG_OPEN");fflush(stdout);
    for(unsigned i=0;i<250&&!done;i++){cellSysutilCheckCallback();usleep(100000);}
    if(!done)cellMsgDialogAbort();
    cellSysutilUnregisterCallback(0);
    printf("H3MAP_EXIT %d\n",success?0:1);fflush(stdout);
    return success?0:1;
}

