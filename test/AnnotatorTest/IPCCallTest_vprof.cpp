// VProfiler included header
#include "VProfEventWrappers.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

int main() {
    int pipefds[2];
    void *buff;
    key_t key;

    pipe_vprofiler(pipefds);
    write_vprofiler(pipefds[0], buff, 5);
    read_vprofiler(pipefds[1], buff, 5);

    int msqid = msgget_vprofiler(key, 438 | 512);
    int i = 0;
    msgsnd_vprofiler(msqid, &i, sizeof(int), 0);
    msgrcv_vprofiler(msqid, &i, sizeof(int), 2, 0);

    mknod_vprofiler("myfifo", 4096 | 420, 0);
    int fifofd1 = open_vprofiler("myfifo", 1);
    int fifofd2 = open_vprofiler("myfifo", 0);
    write_vprofiler(fifofd1, buff, 5);
    read_vprofiler(fifofd2, buff, 5);

    return 0;
}
