#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <stdio.h>
#include <string.h>


int main() {
    char *fifo_name = "fifo_channel";

    int err = mknod(fifo_name, S_IFIFO | 0644, (dev_t) 0);
    if (err != 0 && errno != EEXIST) {
        perror("mknod fails");
    }
    int fd = open(fifo_name, O_RDWR);
    pid_t pid = fork();
    char buf[200];
    memset(buf, 0, sizeof(buf));
    if (pid != 0) {
        // Parent
        char *test_string = "Test";
        size_t len = strlen(test_string);
        strncpy(buf, test_string, len);
        write(fd, buf, len + 1);
    } else {
        int sread = read(fd, buf, sizeof(buf));
        printf("Message: %d, %s\n", sread, buf);
    }
    close(fd);
    return 0;
}
