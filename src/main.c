#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/process.h>
#include <cell/sysutil.h>
#include <cell/msg_dialog.h>

SYS_PROCESS_PARAM(1001, 0x10000);
static volatile int finished;
static volatile int answered;
static void answer(int button, void *data) {
    (void)data;
    answered = 1;
    finished = 1;
    printf("H3PS3_DIALOG_RESPONSE=%d\n", button);
}
static void event(uint64_t status, uint64_t param, void *data) {
    (void)param; (void)data;
    if (status == CELL_SYSUTIL_REQUEST_EXITGAME) finished = 1;
}
int main(void) {
    int rc = cellSysutilRegisterCallback(0, event, NULL);
    if (rc) { printf("Callback error: %08x\n", (unsigned)rc); return 1; }
    unsigned type = CELL_MSGDIALOG_TYPE_SE_TYPE_NORMAL |
        CELL_MSGDIALOG_TYPE_BG_VISIBLE |
        CELL_MSGDIALOG_TYPE_BUTTON_TYPE_YESNO |
        CELL_MSGDIALOG_TYPE_DEFAULT_CURSOR_YES;
    rc = cellMsgDialogOpen2(type,
        "H3-PS3: Hola PlayStation 3! Prueba experimental. Selecciona Si o No para salir.",
        answer, NULL, NULL);
    if (rc) {
        printf("Dialog error: %08x\n", (unsigned)rc);
        cellSysutilUnregisterCallback(0);
        return 2;
    }
    puts("H3PS3_DIALOG_OPENED");
    for (unsigned i = 0; i < 1200 && !finished; ++i) {
        rc = cellSysutilCheckCallback();
        if (rc) { printf("Event error: %08x\n", (unsigned)rc); break; }
        usleep(100000);
    }
    if (!answered) cellMsgDialogAbort();
    cellSysutilUnregisterCallback(0);
    puts(answered ? "H3PS3_RESPONSE_RECEIVED" : "H3PS3_NO_RESPONSE");
    return answered ? 0 : 3;
}
