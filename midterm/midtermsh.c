#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string>
#include <signal.h>
#include <vector>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>


using namespace std;

const int BUFF_SIZE(4096);
pid_t *pid_list,child_pid;
size_t pl_size;

void my_handler(int sig) {
    if (child_pid != 0) {
        for (int i = 0; i < pl_size; ++i) {
            kill(pid_list[i],SIGINT);
        }
    }

}




int read_all(char *buf, int sz) {
    int size = 0, inc;
    while (size < sz - 1 ) {
        inc = read(STDIN_FILENO, buf + size, sz - size - 1);

        if (inc <= 0 ) {
            break;
        }
        size += inc;
        buf[size] = '\0';

        if (strchr(&(buf[size - inc]),'\n') != NULL) {
            break;
        }
    }
    return size;
}


vector<vector<string>> data_list;
char toReadBuffer[BUFF_SIZE];
string sBuf;

void  parse() {
    int  pos, save = 0;
    while ((pos = sBuf.find('\n')) == string::npos) {
        read_all(toReadBuffer,BUFF_SIZE);
        sBuf += string(toReadBuffer);
    }
    string curr = sBuf.substr(0,pos+1);
    sBuf = sBuf.substr(pos+1);
    data_list.clear();
    string s;
    while ((pos = curr.find('\n',save)) != string::npos) {


        data_list.push_back(vector<string>());
        int prev = save,next;
        while (1) {
            while (curr[prev] == ' ') {
                ++prev;
            }
            next = prev;
            while (curr[next] != ' ' && curr[next] != '|' && curr[next] != '\n') {
                ++next;
            }
            string tmps = curr.substr(prev,next - prev);
            data_list.back().push_back(tmps);
            if (curr[next] == '|' || curr[next] == '\n') {
                break;
            }
            prev = next;
        }
        save = next + 1;
    }

}

int main() {
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

        printf("\n$");

        parse();
        pid_list = (pid_t*) malloc(data_list.size() * sizeof(pid_t));


        pl_size = 0;
        fdin = dup(STDIN_FILENO);
        int tsize = data_list.size();

        char *** exArgs =(char***) new char[tsize];

        for (int i =0; i < data_list.size(); ++i) {
            exArgs[i] =(char**) new char[data_list[i].size() + 1];

            for (int j = 0; j < data_list[i].size(); ++j) {
                auto t1 = data_list[i][j];
                exArgs[i][j] = new char[t1.size() + 1];
                strcpy(exArgs[i][j],t1.c_str());
            }
        }
        for (int i = 0; i < data_list.size(); ++i) {


            if (i + 1 == data_list.size()) {
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
                execvp(exArgs[i][0], exArgs[i]);
                _exit(1);
            }

            ++pl_size;
            pid_list[i] = child_pid;

            close(fdin);
            close(fdout);
            fdin = pipefd[0];
        }

        for (int i = 0; i < data_list.size(); ++i) {
            if (waitpid(pid_list[i],NULL,0) < 0) {
                perror(">>>");
            }
            
            free(exArgs[i]);
        }
        free(exArgs);
        free(pid_list);
    }
    return 0;
}

