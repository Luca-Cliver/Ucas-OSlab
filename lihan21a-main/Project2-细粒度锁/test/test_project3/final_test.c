#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#define BUF_LEN 30

int main(int argc, char *argv[])
{
    int print_location = (argc == 1) ? 0 : atoi(argv[1]);

    pid_t fly_pid = sys_exec("fly", 0, 0);
    sys_taskset_bypid(fly_pid, 1);

    char buf_lock_location1[BUF_LEN];
    char buf_lock_handle1[BUF_LEN];
	assert(itoa(print_location + 1, buf_lock_location1, BUF_LEN, 10) != -1);
    char *argv1[3] = {"fine-grained-lock", \
                       buf_lock_location1, \
                       buf_lock_handle1 \
                       };
    pid_t lock_pid1 = sys_exec("fine-grained-lock", 3, argv1);
    sys_taskset_bypid(lock_pid1, 2);

    char buf_lock_location2[BUF_LEN];
    char buf_lock_handle2[BUF_LEN];
	assert(itoa(print_location + 3, buf_lock_location2, BUF_LEN, 10) != -1);
    char *argv2[3] = {"fine-grained-lock", \
                       buf_lock_location2, \
                       buf_lock_handle2 \
                       };
    pid_t lock_pid2 = sys_exec("fine-grained-lock", 3, argv2);
    sys_taskset_bypid(lock_pid2, 1);

    sys_waitpid(lock_pid1);
    sys_waitpid(lock_pid2);

    return 0;
}