#include <common.h>
#include <asm.h>
#include <os/kernel.h>
#include <os/task.h>
#include <os/string.h>
#include <os/loader.h>
#include <type.h>

#define VERSION_BUF 50
#define MAXNAME 8
#define TASK_MAXNUM      16

int version = 2; // version must between 0 and 9
char buf[VERSION_BUF];

// Task info array
task_info_t tasks[TASK_MAXNUM];

static int bss_check(void)
{
    for (int i = 0; i < VERSION_BUF; ++i)
    {
        if (buf[i] != 0)
        {
            return 0;
        }
    }
    return 1;
}

static void init_jmptab(void)
{
    volatile long (*(*jmptab))() = (volatile long (*(*))())KERNEL_JMPTAB_BASE;

    jmptab[CONSOLE_PUTSTR]  = (long (*)())port_write;
    jmptab[CONSOLE_PUTCHAR] = (long (*)())port_write_ch;
    jmptab[CONSOLE_GETCHAR] = (long (*)())port_read_ch;
    jmptab[SD_READ]         = (long (*)())sd_read;
}

static void init_task_info(void)
{
    // TODO: [p1-task4] Init 'tasks' array via reading app-info sector
    // NOTE: You need to get some related arguments from bootblock first
    short taskinfo_blockid = *(short *)0x502001fa;
    bios_sd_read(0x50400000, 1, taskinfo_blockid);

    task_info_t *cur_task = (task_info_t *)0x50400000;
    for(int i = 0; i < TASK_MAXNUM; ++i)
    {
        for(int j = 0; j < TASK_MAXNUM; ++j)
        {
            tasks[i].task_name[j] = cur_task[i].task_name[j];
        }
        tasks[i].task_offset = cur_task[i].task_offset;
        tasks[i].task_size = cur_task[i].task_size;
        tasks[i].task_entry = cur_task[i].task_entry;
    }
}

/************************************************************/
/* Do not touch this comment. Reserved for future projects. */
/************************************************************/

int main(void)
{
    // Check whether .bss section is set to zero
    int check = bss_check();



    // Init jump table provided by kernel and bios(ΦωΦ)
    init_jmptab();

    // Init task information (〃'▽'〃)
    init_task_info();

    // Output 'Hello OS!', bss check result and OS version
    char output_str[] = "bss check: _ version: _\n\r";
    char output_val[2] = {0};
    int i, output_val_pos = 0;

    output_val[0] = check ? 't' : 'f';
    output_val[1] = version + '0';
    for (i = 0; i < sizeof(output_str); ++i)
    {
        buf[i] = output_str[i];
        if (buf[i] == '_')
        {
            buf[i] = output_val[output_val_pos++];
        }
    }

    bios_putstr("Hello OS!\n\r");
    bios_putstr(buf);

    // TODO: Load tasks by either task id [p1-task3] or task name [p1-task4],
    //   and then execute them.
    /*p1 task2*/
    /*char keep_input;
    while(1)
    {
        keep_input = bios_getchar();
        if(keep_input == -1)
        {
            ;
        }
        else if(keep_input >= '0' && keep_input <= '9')
        {
            int num = keep_input - '0';
            bios_putchar(keep_input);
        }
        else if(keep_input >= 'a' && keep_input <= 'z' || keep_input >= 'A' && keep_input <= 'Z')
        {
            bios_putchar(keep_input);
        }
        else if (keep_input == 13)
        {
            bios_putstr("\n\r");
        }
        else if (keep_input == 127)
        {
            bios_putstr("\b");
        }
    }*/
    /*p1 task3*/
/*  while(1)
    {
        volatile int input_taskid = 0;
        volatile int legal = 1;
        volatile int enter_addr;
        volatile int x = 0;
        while(1)
        {
            x = port_read_ch();
            while(x == -1)
                x = port_read_ch();
            port_write_ch(x);
            if(x == '\n' || x == '\r')
                break;
            else if(x > '9' || x < '0')
            {
                bios_putstr("Wrong taskid\n\r");
                legal = 0;
                break;
            }
            input_taskid = input_taskid * 10 + x - '0';
        }
        if(legal)
        {
            if(input_taskid > 3)
            {
                bios_putstr("Wrong taskid,too large\n\r");
            }
            else
            {
                bios_putstr("task started\n\r");
                enter_addr = load_task_img(input_taskid);
                asm volatile(
                    "jalr %[addr]"
                    ::[addr] "r" (enter_addr)
                );
                bios_putstr("task finished\n\r");
            }
        }
    }*/
    /*p1 task4*/
    volatile int x = 0;
    char temp_name[MAXNAME];
    int temp_name_len = 0;
    volatile int enter_addr;
    while(1)
    {
        int count_task = 0;
        x = bios_getchar();
        if(x == -1)
        {
            ;
        }
        else if(x == '\r' || x == '\n')
        {
            bios_putchar('\n');
            for(int j = 0; j < TASK_MAXNUM; j++)
            {
                if(strcmp(tasks[j].task_name, temp_name) == 0 && temp_name[0] != '\0' || temp_name[0] == j + '0' && temp_name[1] == '\0')
                {
                    enter_addr = load_task_img(j);
                    bios_putstr("task start\n");
                   asm volatile(
                        "jalr %[addr]"
                        ::[addr] "r" (enter_addr)
                    );
                    bios_putstr("task finish\n");
                    break;
                }
                else
                    count_task++;

                if(count_task == TASK_MAXNUM)
                {
                    bios_putstr("Wrong name\n");
                    count_task = 0;
                }
            }
            for(int j = 0; j < MAXNAME; j++)
            {
                temp_name[j] = '\0';
            }
            temp_name_len = 0;
        }
        else if(x == " ")
        {
            bios_putchar(' ');
        }
        else if(x == 127)
        {
            bios_putchar('\b');
            temp_name_len--;
            temp_name[temp_name_len] = '\0';
        }
        else 
        {
            bios_putchar(x);
            temp_name[temp_name_len++] = x;
        }
    }
    // Infinite while loop, where CPU stays in a low-power state (QAQQQQQQQQQQQ)
    while (1)
    {
        asm volatile("wfi");
    }

    return 0;
}
