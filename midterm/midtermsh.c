#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <stdio.h>


int BUFF_SIZE = 4096;
pid_t *pid_list,child_pid;
size_t pl_size;

void my_handler(int sig) {
	if (child_pid != 0) {
		for (int i = 0; i < pl_size; ++i) {
            kill(pid_list[i],SIGINT);
        }
	}

}


struct ProgramData {
    char** __data;
    int data_size;
};



void write_all(char *buf, int sz) {
    int shift = 0;
    while (sz != shift) {
        shift +=  write(STDOUT_FILENO, buf + shift, sz - shift);
    }
}
int read_all(char *buf, int sz) {
    int size = 0, inc;
    while (size < sz - 1 ) {
        size += inc = read(STDIN_FILENO, buf + size, 64);
        if (size == 0 || buf[size - 1] == '\n') {
            break;
        }
    };
    buf[size] = '\0';
	return size;
}


struct ProgramData* create_ProgramData(char* s, int sz) {
        struct ProgramData* curr = (struct ProgramData*) malloc(sizeof(struct ProgramData));
        int pos = 0,count = 1;
        while (pos + 1 < sz) {
            if (s[pos] == ' ' && s[pos + 1] != ' ') {
                ++count;
            }
            ++pos;
        }
        pos = 0;
        curr->__data = (char**) malloc((count + 1) * sizeof(char*));
        int i = 0;
        while (i < count) {

            while(pos < sz && s[pos] == ' ' ) {
                ++pos;
            } 
            int tmp = pos;
            while(pos < sz && s[pos] != ' ') {
                ++pos;
            }
            char* buf;
            curr->__data[i] = buf = (char*) malloc((pos - tmp + 1) * sizeof(char));
            strncpy(curr->__data[i],s + tmp,pos - tmp);
            buf[pos - tmp] = '\0';
            ++i;   
        }
        curr->data_size = count;
        curr->__data[count] = NULL;
        return curr;
    }

void free_ProgramData(struct ProgramData *ptr) {
    for (int i = 0; i < ptr->data_size; ++i) {
        free(ptr->__data[i]);
    }
    free(ptr->__data);
    free(ptr);
}

struct ProgramData** data_list;

int parse() {
    char* buf = (char*) malloc(BUFF_SIZE * sizeof(char)), *ptr,*prev_ptr;
    int sz = read_all(buf,BUFF_SIZE-1),count = 1;

    if (sz == 0) {
        exit(0);
    }

    prev_ptr = buf;
    while ((ptr = strchr(prev_ptr + 1,'|')) != NULL) {
        prev_ptr = ptr;
        ++count;
    }
    data_list = (struct ProgramData**) malloc(count * sizeof(struct ProgramData*));
    prev_ptr = buf;
    for (int i = 0; i < count; ++i)
    {
        while (*prev_ptr == ' ') {
            prev_ptr = prev_ptr + 1;
        }
        ptr = strchr(prev_ptr,'|');
        if (ptr == NULL) {
            ptr = buf + sz - 1; 
        }
        data_list[i] = create_ProgramData(prev_ptr, ptr - prev_ptr);
        prev_ptr = ptr + 1;
    }
    free(buf);
    return count;

}

int main() {
    char *inv = "\n$";

    struct sigaction sa;
    sa.sa_handler = my_handler;
    sigset_t ss;
    sigemptyset(&ss);
    sa.sa_mask = ss;
    sa.sa_flags = 0;
    sigaction(SIGINT,&sa,NULL);

    int pipefd[2];
    int fdin,fdout;

    while (1) {

        write_all(inv, 2);

        int val = parse();
        pid_list = (pid_t*) malloc(val * sizeof(pid_t));
        pl_size = 0;
        fdin = dup(STDIN_FILENO);

        for (int i = 0, border = val; i < border; ++i) {
           if (i + 1 == border) {
                fdout = dup(STDOUT_FILENO); 
           } else {
                pipe(pipefd);
                fdout = pipefd[1];
           }

           child_pid = fork();

           if (child_pid == 0){ 

                dup2(fdin,STDIN_FILENO);
                dup2(fdout,STDOUT_FILENO);

                close(fdin);
                close(fdout);

                execvp(data_list[i]->__data[0], data_list[i]->__data);
                _exit(1);
            }

            ++pl_size;
            pid_list[i] = child_pid;

            close(fdin);
            close(fdout);
            fdin = pipefd[0];
        }

        for (int i = 0; i < val; ++i) {
            if (waitpid(pid_list[i],NULL,0) < 0) {
                perror(">>>");
            }
            free_ProgramData(data_list[i]);
        }
        free(pid_list);
        free(data_list);
    }
    return 0;
}
