#include <stdio.h>
#include <string.h>
#include <unistd.h>

// whence of do_lseek 
enum Lseek_whence
{
    SEEK_SET,
    SEEK_CUR,
    SEEK_END
};

static char buff[64];
#define O_RDWR 3
int main(void)
{
    int fd = sys_fopen("1.txt", O_RDWR);
    

    // write 'hello world!' * 10
    for (int i = 0; i < 12; i++)
    {
        sys_lseek(fd, 1000000*i, SEEK_SET);
        sys_fwrite(fd, "hello world!", 13);
        sys_fread(fd, buff, 13);
        for (int j = 0; j < 13; j++)
        {
            printf("%c", buff[j]);
        }
    }

    sys_fclose(fd);

    return 0;
}