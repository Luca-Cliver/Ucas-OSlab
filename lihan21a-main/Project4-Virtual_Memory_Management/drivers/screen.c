#include <screen.h>
#include <printk.h>
#include <os/string.h>
#include <os/sched.h>
#include <os/irq.h>
#include <os/kernel.h>
#include <os/smp.h> // for [p3-task3]

#define SCREEN_WIDTH    80
// modified in [p3-task1]
// #define SCREEN_HEIGHT   50
#define SCREEN_HEIGHT   40
#define SCREEN_LOC(x, y) ((y) * SCREEN_WIDTH + (x))

/* screen buffer */
char new_screen[SCREEN_HEIGHT * SCREEN_WIDTH] = {0};
char old_screen[SCREEN_HEIGHT * SCREEN_WIDTH] = {0};

spin_lock_t screen_lock ={
    .status = 0,
    .owner = -1
}; // for [p3-task3]

/* cursor position */
static void vt100_move_cursor(int x, int y)
{
    // \033[y;xH
    printv("%c[%d;%dH", 27, y, x);
}

/* clear screen */
static void vt100_clear()
{
    // \033[2J
    printv("%c[2J", 27);
}

/* hidden cursor */
static void vt100_hidden_cursor()
{
    // \033[?25l
    printv("%c[?25l", 27);
}

/* write a char */
static void screen_write_ch(char ch)
{
    //spin_lock_acquire(&screen_lock);
    if (ch == '\n')
    {
        current_running[get_current_cpu_id()]->cursor_x = 0;
        current_running[get_current_cpu_id()]->cursor_y++;

        // for [p3-task1]
        if(current_running[get_current_cpu_id()]->cursor_y==SCREEN_HEIGHT-1){
            screen_scroll(20); // SHELL_BEGIN is defined in shell.c
            current_running[get_current_cpu_id()]->cursor_y--;
        }
    }
    else
    {
        new_screen[SCREEN_LOC(current_running[get_current_cpu_id()]->cursor_x, current_running[get_current_cpu_id()]->cursor_y)] = ch;
        current_running[get_current_cpu_id()]->cursor_x++;
    }
    //spin_lock_release(&screen_lock);
}

void init_screen(void)
{
    // for [p3-task3]
    //spin_lock_init(&screen_lock);

    vt100_hidden_cursor();
    vt100_clear();
    screen_clear();
}

void screen_clear(void)
{
    //spin_lock_acquire(&screen_lock);
    int i, j;
    for (i = 0; i < SCREEN_HEIGHT; i++)
    {
        for (j = 0; j < SCREEN_WIDTH; j++)
        {
            new_screen[SCREEN_LOC(j, i)] = ' ';
        }
    }
    current_running[get_current_cpu_id()]->cursor_x = 0;
    current_running[get_current_cpu_id()]->cursor_y = 0;
    //spin_lock_release(&screen_lock);

    screen_reflush();
}

void screen_move_cursor(int x, int y)
{
    current_running[get_current_cpu_id()]->cursor_x = x;
    current_running[get_current_cpu_id()]->cursor_y = y;
    vt100_move_cursor(x, y);
}

void screen_write(char *buff)
{
    int i = 0;
    int l = strlen(buff);

    for (i = 0; i < l; i++)
    {
        screen_write_ch(buff[i]);
    }
}

/*
 * This function is used to print the serial port when the clock
 * interrupt is triggered. However, we need to pay attention to
 * the fact that in order to speed up printing, we only refresh
 * the characters that have been modified since this time.
 */
void screen_reflush(void)
{
    //spin_lock_acquire(&screen_lock);
    int i, j;

    /* here to reflush screen buffer to serial port */
    for (i = 0; i < SCREEN_HEIGHT; i++)
    {
        for (j = 0; j < SCREEN_WIDTH; j++)
        {
            /* We only print the data of the modified location. */
            if (new_screen[SCREEN_LOC(j, i)] != old_screen[SCREEN_LOC(j, i)])
            {
                vt100_move_cursor(j + 1, i + 1);
                bios_putchar(new_screen[SCREEN_LOC(j, i)]);
                old_screen[SCREEN_LOC(j, i)] = new_screen[SCREEN_LOC(j, i)];
            }
        }
    }

    /* recover cursor position */
    vt100_move_cursor(current_running[get_current_cpu_id()]->cursor_x, current_running[get_current_cpu_id()]->cursor_y);
    //spin_lock_release(&screen_lock);
}

void do_backspace()
{
    uint64_t cpu_id = get_current_cpu_id();
    current_running[cpu_id]->cursor_x--;
    screen_write_ch(' ');
    screen_reflush();
    current_running[cpu_id]->cursor_x--;
    vt100_move_cursor(current_running[cpu_id]->cursor_x, current_running[cpu_id]->cursor_y);
}



// for [p3-task1]
void screen_scroll(int shell_begin){
    for(int i=shell_begin+1; i<SCREEN_HEIGHT-1; i++)
    {
        for(int j=0; j<SCREEN_WIDTH; j++)
        {
            new_screen[SCREEN_LOC(j, i)] = new_screen[SCREEN_LOC(j, i+1)];
        }
    }
    for(int j=0; j<SCREEN_WIDTH; j++)
    {
        new_screen[SCREEN_LOC(j,SCREEN_HEIGHT-1)] = ' ';
    }
}