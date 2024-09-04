#include <stdio.h>
#include <syscall.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#define test_num 250


int main()
{
    sys_move_cursor(1, 1);
    printf("Begin page swap testing. Please note swap infomation.\n");
    long* pf1 = 0x10000000;
    long* pf2 = 0x20000000;
    long* pf3 = 0x30000000;
    long* pf4 = 0x40000000;
    long* pf5 = 0x50000000;
    long* pf6 = 0x60000000;
    *pf1 = test_num;
    sys_move_cursor(30, 2);
    printf("Modify pf1\n");
    *pf2 = test_num;
    sys_move_cursor(30, 3);
    printf("Modify pf2\n");
    *pf3 = test_num;
    sys_move_cursor(30, 4);
    printf("Modify pf3\n");
    *pf4 = test_num;
    sys_move_cursor(30, 5);
    printf("Modify pf4\n");
    *pf5 = test_num;
    sys_move_cursor(30, 6);
    printf("Modify pf5\n");
    *pf6 = test_num;
    sys_move_cursor(30, 7);
    printf("Modify pf6\n");
    if(*pf1 != test_num){
        printf("Error, pf1!\n");
        return 0;
    }
    if(*pf2 != test_num){
        printf("Error, pf2!\n");
        return 0;
    }
    if(*pf3 != test_num){
        printf("Error, pf3!\n");
        return 0;
    }
    if(*pf4 != test_num){
        printf("Error, pf4!\n");
        return 0;
    }
    if(*pf5 != test_num){
        printf("Error, pf5!\n");
        return 0;
    }
    if(*pf6 != test_num){
        printf("Error, pf6!\n");
        return 0;
    }
    sys_move_cursor(30,8);
    printf("Test Succeed!\n");
    sys_move_cursor(30,9);
    printf("End testing.\n");
    return 0;
}