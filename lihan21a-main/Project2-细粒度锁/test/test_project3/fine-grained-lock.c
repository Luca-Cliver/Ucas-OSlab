#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>

#define LOCK_KEY 24

int main(int argc, char *argv[])
{
    sys_waitpid(atoi(argv[2]));
    
    int print_location = (argc == 1) ? 0 : atoi(argv[1]);
    int id = sys_mutex_init(LOCK_KEY);

    clock_t lock_begin = clock();
    for(int i = 0; i < 10000; i++)
    {
        sys_mutex_acquire(id);
        sys_mutex_release(id);
        sys_move_cursor(0, print_location);
		printf(" acquire and release lock %d times", i + 1);
    }
    clock_t lock_end = clock();

    sys_move_cursor(0, print_location + 1);
    printf("over, total %ld ticks", lock_end - lock_begin);
    return 0;
}