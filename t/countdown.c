#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#define sleep(t) Sleep((t)*1000)
#endif

int main(int argc, char** argv) {
    int ns=0;
    if(argc<2) return 1;
    ns=atoi(argv[1]);
    while(ns) {
        printf("%d\n", ns);
        --ns;
        fflush(stdout);
        sleep(1);
    }
    printf("start!\n");
    return 0;
}
