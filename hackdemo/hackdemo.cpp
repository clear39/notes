#include<stdio.h>
#include <unistd.h>

static bool global_bool_val = false;

static int global_int_val = 0xFEEFABBA;

int main() {
    printf("Hello World %d %d \n",global_bool_val,global_int_val);
    while(1){
       sleep(1);
    }
    return 0;
}
