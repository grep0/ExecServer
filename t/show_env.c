#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv) {
    int i;
    for(i=1; i<argc; ++i) {
    	const char* k=argv[i];
    	const char* v=getenv(k);
    	if(v) printf("%s=%s\n", k,v);
    	else  printf("%s not set\n", k);
    }
    return 0;
}
