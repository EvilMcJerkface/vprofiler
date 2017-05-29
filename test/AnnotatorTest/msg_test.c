#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <stdio.h>
#include <string.h>
typedef struct msgbuf {
    long mtype;
    char mtext[200];
} msgbuf;


int main() {
    int msqid = msgget(IPC_PRIVATE, 0777 | IPC_CREAT);
    if (msqid == -1) {
        puts("msgget fails");
    }
    pid_t pid = fork();
    msgbuf msg;
    memset(msg.mtext, 0, sizeof(msg.mtext));
    if (pid != 0) {
        // Parent
        char *test_string = "Test";
        size_t len = strlen(test_string);
        msg.mtype = 1;
        strncpy(msg.mtext, test_string, len);
        msgsnd(msqid, &msg, sizeof(msg.mtext), 0);
    } else {
        int sread = msgrcv(msqid, &msg, sizeof(msg.mtext), 0, 0);
        printf("Message: %d, %s\n", sread, msg.mtext);
        int rc = msgctl(msqid, IPC_RMID, NULL);
        if (rc < 0) {
            perror( strerror(errno) );
            printf("msgctl (return queue) failed, rc=%d\n", rc);
            return 1;
        }
    }
    return 0;
}
