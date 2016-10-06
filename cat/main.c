#include <unistd.h>
#include <fcntl.h>

#define BUFF_SIZE 4096

int read_all(char* buf,int max_size, int desc) {
	int sz = 0, inc;
	while(sz < max_size){
		inc=read(desc,buf+sz,max_size - sz);
		if (inc <= 0) {
			break;
		}
		sz += inc;
	}
	return sz;
}

int write_all(char* buf,int sz) {
	int count,size = 0;
	while(size != sz) {
		count=write(STDOUT_FILENO,buf + size,sz - size);
		if (count <= 0) {
			break;
		}
		size += count;
	}
	return size;
}


int main(int count,char** args) {
	int sz;
	char buf[BUFF_SIZE];
	if (count == 1) {
		while (1) {
			sz = read(STDIN_FILENO,buf,BUFF_SIZE);
			if (sz <= 0) {
				break;
			}
			write_all(buf,sz);
		}
	}
	for (int i =1,desc; i < count;++i) {
		if((desc = open(args[i],O_CREAT|O_RDONLY)) < 0) {
			break;
		}
		while(1) {
			sz = read_all(buf,BUFF_SIZE,desc);
			write_all(buf,sz);
			if (sz != BUFF_SIZE) {
				break;
			}
		}
	}
}
