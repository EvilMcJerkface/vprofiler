// VProfiler included header
#include "VProfEventWrappers.h"

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <stdio.h>
#include <string.h>


int main() {
    int pipefds[2];
    pipe_vprofiler(pipefds);
    pid_t pid = fork();
    char buf[200];
    memset(buf, 0, sizeof(buf));
    if (pid != 0) {
        // Parent
        close(pipefds[0]);
        char *test_string = "Test";
        size_t len = strlen(test_string);
        strncpy(buf, test_string, len);
        write_vprofiler(pipefds[1], buf, len + 1);
        close(pipefds[1]);
    } else {
        close(pipefds[1]);
        int sread = read_vprofiler(pipefds[0], buf, sizeof (buf));
        printf("Message: %d, %s\n", sread, buf);
        close(pipefds[0]);
    }
    return 0;
}
