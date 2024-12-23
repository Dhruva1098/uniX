#include <stdio.h>

int main(int argc, char** argv){
    
    if(argc < 3) {
        printf("Invalid syntax. Syntax: %s <disk image> <file name> \n", argv[0]);
        return 1;
    }
    else return 0;
}
