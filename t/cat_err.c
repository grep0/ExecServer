#include <stdio.h>

int main(int argc, char** argv) {
    char buf[1000];
    while(1) {
    	int nr=fread(buf,1,sizeof(buf),stdin);
    	if(nr==0) break;
    	fwrite(buf,1,nr,stdout);
    	fwrite(buf,1,nr,stderr);
    }
    return 0;
}
