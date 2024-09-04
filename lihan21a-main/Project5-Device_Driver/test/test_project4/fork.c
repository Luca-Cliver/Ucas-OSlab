#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int main(int argc, char *argv[])
{
	int loc = 2;
	pid_t pid;

	sys_move_cursor(0, loc);
	pid = sys_fork();
    //pid = 0;
	if (pid < 0)
	{
        sys_move_cursor(0,1);
		printf("sys_fork() failed\n");
		return 1;
	}
	if (pid > 0)
    {
        sys_move_cursor(0,6);
		printf("I am parent process pid == %d\n", pid);
    }
	else
    {
        sys_move_cursor(0,2);
		printf("\n\nI am child process pid == %d",pid);
    }
	return 0;
}