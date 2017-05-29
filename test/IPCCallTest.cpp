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

    pipe(pipefds);
    write(pipefds[0], buff, 5);
    read(pipefds[1], buff, 5);

    int msqid = msgget(key, 0666 | IPC_CREAT);
    int i = 0;
    msgsnd(msqid, &i, sizeof(int), 0);
    msgrcv(msqid, &i, sizeof(int), 2, 0);

    mknod("myfifo", S_IFIFO | 0644, 0);
    int fifofd1 = open("myfifo", O_WRONLY);
    int fifofd2 = open("myfifo", O_RDONLY);
    write(fifofd1, buff, 5);
    read(fifofd2, buff, 5);

    return 0;
}
