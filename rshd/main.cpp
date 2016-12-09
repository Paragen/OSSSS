#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <sys/epoll.h>
#include <map>
#include <set>
#include <signal.h>
#include <string>
#include <string.h>
#include <sstream>

#define MAX_SERV 100
#define BUFF_SIZE 4096


using namespace std;

int openPT() {
    int ptFd, errBuf;


    if ((ptFd = posix_openpt(O_RDWR|O_NOCTTY)) == -1) {  // get pr master fd
        return -1;
    }

    if (unlockpt(ptFd) == -1) { // unlock pt slave
        errBuf = errno;
        close(ptFd);
        errno = errBuf;
        return -1;
    }


    return ptFd;
}

pid_t ptFork(int fd) {
    char* ptsName;
    if ((ptsName = ptsname(fd) ) == NULL) {  // get pt slave name
        close(fd);
        return -1;
    }

    int pid = fork();

    if (pid == 0) {

        close(fd);

        if (setsid() == -1) {  // create new session
            return -1;
        }

        int sFd;

        if ((sFd = open(ptsName,O_RDWR)) < 0 ) { // create slave fd
            return -1;
        }

        if (dup2(sFd,STDIN_FILENO) != STDIN_FILENO) { // set fd's to sFd
            return -1;
        }
        if (dup2(sFd,STDOUT_FILENO) != STDOUT_FILENO) {
            return -1;
        }
        if (dup2(sFd,STDERR_FILENO) != STDERR_FILENO) {
            return -1;
        }

        fd = sFd;
        return pid;
    }
    return pid;
}

pid_t addClientPT(int fd) {
    pid_t pid = ptFork(fd);
    switch (pid) {
    //error
    case -1 :
        _exit(EXIT_FAILURE);
        //child
    case 0 :
        execlp("/bin/sh","/bin/sh",(char*)NULL); // run shell
        _exit(EXIT_SUCCESS);
    default:
        return pid;
    }

}

int makeDaemon() {

    switch (fork()) {
    case 0: break;
    case -1: return -1;
    default: exit(EXIT_SUCCESS);
    }

    if (setsid() == -1) {
        return -1;
    }

    switch (fork()) {
    case 0: break;
    case -1: return -1;
    default: exit(EXIT_SUCCESS);
    }

    chdir("~");
    close(STDIN_FILENO);
    int fd;
    if ((fd = open("/dev/null",O_RDWR)) < 0) {
        return -1;
    }
    if (dup2(fd,STDOUT_FILENO) < 0) {
        return -1;
    }
    if (dup2(fd,STDERR_FILENO) < 0) {
        return -1;
    }

    if ((fd = open("/var/run/rshd.pid",O_RDWR)) > 0) {\
        dprintf(fd,"%d",(int)getpid());
        close(fd);
    }
    return 0;
}

struct dInfo {
    int fd,flags;
    dInfo* neighbor;
    char* buf;
    size_t size;
    dInfo(int fd):fd(fd),neighbor(nullptr),size(0) {
        buf = new char[BUFF_SIZE];
    }
    ~dInfo() {
        delete[] buf;
    }
};

int epollAdd(int efd, dInfo* ptr) {
    epoll_event ev;
    ev.data.ptr = (void*) ptr;
    ptr->flags = ev.events = EPOLLIN;

    if (epoll_ctl(efd,EPOLL_CTL_ADD,ptr->fd,&ev) < 0) {
        if (ptr != nullptr) {
            delete ptr;
        }
        return -1;
    }
    return 0;
}



std::map<int,pid_t> pidMap;

void killThemAll() {
    std::set<pid_t> pidSet;
    for (auto it = pidMap.begin(); it != pidMap.end(); ++it) {
        pidSet.insert(it->second);
    }
    for (auto it = pidSet.begin(); it != pidSet.end(); ++it) {
        kill(*it,SIGSTOP);
    }
}

void printLog(string message) {
    FILE* f = fopen("/home/ouroboros/rshdlogs.txt","a");
    fprintf(f,"%s\n",message.c_str());
    fclose(f);
}

void removeClient(dInfo* info,int efd) {
    printLog("Drop with fd:" + std::to_string(info->fd));
    kill(pidMap[info->fd],SIGTERM);
    pidMap.erase(info->fd);
    pidMap.erase(info->neighbor->fd);
    epoll_ctl(efd,EPOLL_CTL_DEL,info->fd,nullptr);
    epoll_ctl(efd,EPOLL_CTL_DEL,info->neighbor->fd,nullptr);
    close(info->fd);
    close(info->neighbor->fd);
    delete info->neighbor;
    delete info;
}

void writeAll(dInfo* info,int efd) {
    int total = 0,val;
    dInfo * neighbor = info->neighbor;
    int save = neighbor->size;
    while(total != neighbor->size) {
        if ((val = write(info->fd,neighbor->buf + total,neighbor->size - total)) <= 0) {
            if (val != 0 && (errno == EWOULDBLOCK || errno == EAGAIN)) {
                break;
            }
            //disconnect
            removeClient(info,efd);
            return;
        }
        total += val;
    }
    std::copy(neighbor->buf + total,neighbor->buf + neighbor->size,neighbor->buf);
    neighbor->size -= total;
    if (neighbor->size == 0 && save > 0 ) {
        epoll_event ev;
        unsigned tmp = ~EPOLLOUT;
        ev.events = info->flags & tmp;
        info->flags = ev.events;
        ev.data.ptr = info;
        if (epoll_ctl(efd,EPOLL_CTL_MOD,info->fd,&ev) < 0) {
            removeClient(info,efd);
        }
    }
    if (neighbor->size < BUFF_SIZE  && save == BUFF_SIZE ) {
        epoll_event ev;
        neighbor->flags = ev.events = (neighbor->flags | EPOLLIN);
        ev.data.ptr = neighbor;
        if (epoll_ctl(efd,EPOLL_CTL_MOD,neighbor->fd,&ev) < 0) {
            removeClient(info,efd);
        }
    }

}

void readAll(dInfo* info, int efd) {
    int val, save = info->size;
    while (info->size < BUFF_SIZE) {
        if ((val = read(info->fd,info->buf + info->size,BUFF_SIZE - info->size)) <= 0){
            if (val != 0 && (errno == EWOULDBLOCK || errno == EAGAIN)) {
                printLog("Error but continue");
                break;
            }
            //disconnect
            removeClient(info,efd);
            return;
        }
        printLog(std::to_string(val));
        info->size += val;
    }
    if (save == 0 && info->size > 0) {
        epoll_event ev;
        ev.events = info->neighbor->flags | EPOLLOUT;
        info->neighbor->flags = ev.events;
        ev.data.ptr = info->neighbor;
        if (epoll_ctl(efd,EPOLL_CTL_MOD,info->neighbor->fd,&ev) < 0) {
            removeClient(info,efd);
        }
    }
    if (save < BUFF_SIZE && info->size == BUFF_SIZE) {
        epoll_event ev;
        int tmp  = ~EPOLLIN;
        info->flags = ev.events = info->flags & tmp;
        ev.data.ptr = info;
        if (epoll_ctl(efd,EPOLL_CTL_MOD,info->fd,&ev) < 0) {
            removeClient(info,efd);
        }
    }
}



int main(int argc, char** args) {

    if (argc != 2) {
        printf("Usage: /rshd port\n");
        return 1;
    }

    if (makeDaemon() < 0) {
        return 1;
    }

    addrinfo hints,*addr, *curr;
    int lfd, efd;

    memset(&hints,0,sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL,args[1],&hints,&addr) < 0) {
        return 2;
    }

    for ( curr = addr; curr != nullptr; curr = curr->ai_next) { //create listen fd
        if ((lfd = socket(curr->ai_family,curr->ai_socktype, curr->ai_protocol)) < 0) {
            continue;
        }

        if (fcntl(lfd,F_SETFL, O_NONBLOCK) < 0) {
            close(lfd);
            continue;
        }

        if (bind(lfd,curr->ai_addr,curr->ai_addrlen) < 0) {
            close(lfd);
            continue;
        }

        if (listen(lfd,10) < 0) {
            close(lfd);
            continue;
        }
        break;
    }

    if (curr == nullptr) { // can't create lfd
        return 3;
    }

    int eListSize = MAX_SERV * 2 + 1;
    epoll_event* eList = new epoll_event[eListSize];

    if ((efd = epoll_create(100)) < 0) {
        _exit(EXIT_FAILURE);
    }

    if (epollAdd(efd,new dInfo(lfd)) < 0) {
        _exit(EXIT_FAILURE);
    }

    int count;

    printLog("RSHD has started");
    while (true) { // main loop
        if ((count = epoll_wait(efd,eList,pidMap.size() + 1,-1)) < 0) {
            if (errno == EINTR) {
                continue;
            }
            killThemAll();
            _exit(EXIT_FAILURE);
        }


        for (int i = 0; i < count; ++i) {
            epoll_event *curr = &eList[i];

            if (((dInfo*)curr->data.ptr)->fd != lfd && pidMap.find(((dInfo*)curr->data.ptr)->fd) == pidMap.end()) {
                continue;
            }

            if (curr->events&EPOLLIN) {

                if (((dInfo*)curr->data.ptr)->fd == lfd ) { //accept
                    int nfd;
                    if (pidMap.size() > 2 * eListSize) {
                        epoll_ctl(efd,EPOLL_CTL_DEL,lfd,NULL);
                        continue;
                    }
                    sockaddr_storage storage;
                    unsigned int stSize;
                    for(;;) {
                        stSize = sizeof storage;
                        if ((nfd = accept(lfd,(sockaddr*)&storage,&stSize)) < 0) {
                            if (errno == EWOULDBLOCK || errno == EAGAIN ) {
                                break;
                            }
                            killThemAll();
                            _exit(EXIT_FAILURE);
                        }

                        printLog("Get new connect");

                        if (fcntl(nfd,F_SETFL,O_NONBLOCK) < 0) {
                            close(nfd);
                            continue;
                        }
                        int fd,pid;
                        fd = openPT();
                        if (fcntl(fd,F_SETFL,O_NONBLOCK) < 0) {
                            close(nfd);
                            close(fd);
                            continue;
                        }
                        pid = addClientPT(fd);

                        pidMap.insert(std::pair<int,pid_t>(fd,pid));
                        pidMap.insert(std::pair<int,pid_t>(nfd,pid));
                        dInfo *first,*second;
                        first = new dInfo(fd);
                        second = new dInfo(nfd);
                        first->neighbor = second;
                        second ->neighbor = first;

                        if (epollAdd(efd,first) < 0 || epollAdd(efd,second) < 0) {
                            killThemAll();
                            _exit(EXIT_FAILURE);
                        }

                        printLog("Get peer with fd:" + std::to_string(nfd) + " and terminal with fd:" + std::to_string(fd));
                    }
                } else {

                    stringstream tmp;
                    tmp << "read from " << ((dInfo*)(curr->data.ptr))->fd << " and lfd ==" << lfd;
                    printLog(tmp.str());
                    readAll((dInfo*)(curr->data.ptr),efd);
                }
                continue;

            }
            if (curr->events&EPOLLOUT) {
                writeAll((dInfo*)(curr->data.ptr),efd);
                continue;
            }
            if (curr->events&EPOLLERR || curr->events&EPOLLHUP) {
                removeClient((dInfo*)curr->data.ptr,efd);
                continue;
            }
        }
    }
    printLog("shut down");
}
