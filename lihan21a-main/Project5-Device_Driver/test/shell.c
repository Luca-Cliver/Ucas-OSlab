/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 *               Author : Han Shukai (email : hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *                  The shell acts as a task running in user mode.
 *       The main function is to make system calls through the user's output.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * */

#include "kernel.h"
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#define SHELL_BEGIN 20
#define MAXLEN 100

void inputParse(char *buffer_temp);

int main(void)
{
    sys_move_cursor(0, SHELL_BEGIN);
    printf("------------------- COMMAND -------------------\n");
    printf("> Cliver@UCAS_OS: ");

    char buffer_temp[MAXLEN];
    for(int i = 0; i < MAXLEN; i++)
    {
        buffer_temp[i] = '\0';
    }
    int buf_len = 0;

    while (1)
    {
        // TODO [P3-task1]: call syscall to read UART port
        int n;
        char c;

        n = sys_getchar();

        
        
        if(n != -1)
        {
            c = (char)n;
            if(c == 8 || c == 127)
            {
                if(buf_len > 0)
                {
                    sys_backspace();
                    buf_len--;
                }
            }
            else if(c == '\r')
            {
                printf("\n");
                if(buf_len < MAXLEN)
                {
                    buffer_temp[buf_len] = '\0';
                    printf("%c", c);
                    inputParse(buffer_temp);
                    buf_len = 0;
                }
                else
                    printf("Error: Unknown command\n");
                printf("> Cliver@UCAS_OS: ");
            }
            else 
            {
                if(buf_len < MAXLEN)
                {
                    buffer_temp[buf_len++] = c;
                    printf("%c", c);
                }
                else
                {    
                    printf("\nError: buffer overflow\n");
                    buf_len = 0;
                    printf("> Cliver@UCAS_OS: ");
                }

            }
        }
        else 
        {
            ;
        }
        
        
        // TODO [P3-task1]: parse input
        // note: backspace maybe 8('\b') or 127(delete)

        // TODO [P3-task1]: ps, exec, kill, clear    

        /************************************************************/
        /* Do not touch this comment. Reserved for future projects. */
        /************************************************************/    
    }

    return 0;
}

int atoi(char *buf){
    int res = 0;
    int i = 0;
    while(buf[i] != '\0'){
       res = res*10 + buf[i] - '0';
       i++;
    }
    return res;
}

void inputParse(char *buffer_temp)
{
    char arg[MAXLEN][MAXLEN];
    int pid_num = 0;
    int tid_num = 0;
    int i = 0;
    int j = 0;

    //获取命令
    while(buffer_temp[i] != '\0' && buffer_temp[i] != ' ')
    {
        arg[0][i] = buffer_temp[j];
        i++;
        j++;
    }
    arg[0][i] = '\0';

    if(buffer_temp[j] == ' ')
    {
        j++;
        i = 0;
        while(buffer_temp[j] != '\0' && buffer_temp[j] != ' ')
        {
            arg[1][i++] = buffer_temp[j++];
        }
        arg[1][i] = '\0';
        pid_num = atoi(arg[1]);
    }

    if(!strcmp(arg[0], "ps"))
    {
        sys_ps();
    }
    else if(!strcmp(arg[0], "clear"))
    {
        sys_clear();
        sys_move_cursor(0, SHELL_BEGIN);
        printf("------------------- COMMAND -------------------\n");
    }
    else if(!strcmp(arg[0], "waitpid"))
    {
        if(!sys_waitpid(pid_num))
            printf("Error: no such process\n");
    }
    else if(!strcmp(arg[0], "exit"))
    {
        sys_exit();
    }
    else if(!strcmp(arg[0], "kill"))
    {
        if(buffer_temp[j] == ' ')
        {
            j++;
            i = 0;
            while(buffer_temp[j] != '\0' && buffer_temp[j] != ' ')
            {
                arg[2][i++] = buffer_temp[j++];
            }
            arg[2][i] = '\0';
            tid_num = atoi(arg[2]);
        }
        else
            tid_num = 0;
        if(!sys_kill(pid_num, tid_num))
            printf("Error: no such process\n");
        else 
            printf("Info: Process pid %d tid %d is killed\n", pid_num, tid_num);
    }
    else if(!strcmp(arg[0], "exec"))
    {
        int pid_to_exec;
        int argc = 0;
        char *argv[8];
        int m, k = 0, wait_mark = 0;
        char *exec_name = arg[1];
        //获取argv
        for(m = 2; m < 9; m++)
        {
            if(buffer_temp[j] == '\0')
                break;
            else if(buffer_temp[j] == ' ')
            {
                k = 0;
                j++;
                while(buffer_temp[j] != '\0' && buffer_temp[j] != ' ')
                {
                    arg[m][k++] = buffer_temp[j++];
                }
            }
            arg[m][k] = '\0';
        }

        if(arg[m-1][0] == '&')
        {
            argc = m - 2;
            wait_mark = 0;
        }
        else 
        {
            argc = m - 1;
            wait_mark = 1;
        }

        for(int n = 0; n < argc + 1; n++)
        {
            argv[n] = arg[n+1];
        }
        if(arg[1][0] == '\0')
            pid_to_exec = 0;
        else
            pid_to_exec = (int)sys_exec(exec_name, argc, argv);
        if(wait_mark)
            sys_waitpid(pid_to_exec);
        if(pid_to_exec)
            printf("Info: Process pid %d is executed\n", pid_to_exec);
        else 
            printf("Error: Exec failed\n");
    }
    else if(!strcmp(arg[0], "taskset"))
    {
        int argc;
        char *argv[1];
        if((!strcmp(arg[1], "0x1")) || (!strcmp(arg[1], "0x2")) || (!strcmp(arg[1], "0x3")))
        {
            if(buffer_temp[j] == ' ')
            {
                j++;
                i = 0;
                while(buffer_temp[j] != '\0' && buffer_temp[j] != ' ')
                {
                    arg[2][i] = buffer_temp[j];
                    i++;
                    j++;
                }
                arg[2][i] = '\0';
            }
            char *taskset_name = arg[2];
            argv[0] = arg[2];
            argc = 1;
            int mask = (!strcmp(arg[1], "0x1"))? 1 :
                       (!strcmp(arg[1], "0x2"))? 2 : 0;
            int pid_to_taskset = (int)sys_taskset(taskset_name, argc, argv, mask);
            if(pid_to_taskset == 0)
            {
                printf("Error: Taskset failed,no such task\n");
            }
        }
        else if(!strcmp(arg[1], "-p"))
        {
            if(buffer_temp[j] == ' ')
            {
                j++;
                i = 0;
                while(buffer_temp[j] != '\0' && buffer_temp[j] != ' ')
                {
                    arg[2][i] = buffer_temp[j];
                    i++;
                    j++;
                }
                arg[2][i] = '\0';
            }

            if((!strcmp(arg[2], "0x1")) || (!strcmp(arg[2], "0x2")) || (!strcmp(arg[2], "0x3")))
            {
                if(buffer_temp[j] == ' ')
                {
                    j++;
                    i = 0;
                    while(buffer_temp[j] != '\0' && buffer_temp[j] != ' ')
                    {
                        arg[3][i] = buffer_temp[j];
                        i++;
                        j++;
                    }
                    arg[3][i] = '\0';
                }
                int pid_to_taskset = atoi(arg[3]);
                int mask = (!strcmp(arg[2], "0x1"))? 1 :
                            (!strcmp(arg[2], "0x2"))? 2 : 0;
                sys_taskset_bypid(pid_to_taskset, mask);
            }
            else
            {
                printf("failed to do taskset, wrong inst");
            }
        }
        else
        {
            printf("failed to do taskset, wrong inst");
        }
    }
    else 
    {
        printf("Error: Unknown command\n");
    }
}
