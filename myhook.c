
// myhook.c
#include <stdlib.h>
// #include <dlfcn.h>
#include <stdio.h>
#include <pthread.h>
#include "myhook.h"
#include "uthread.h"
#include <sys/socket.h>



int pthread_create(pthread_t *tidp,const pthread_attr_t *attr,void *(*start_rtn)(void*),void *arg) {
    
    // int (*mypthread_create)(pthread_t *tidp,const pthread_attr_t *attr,void *(*start_rtn)(void*),void *arg) = dlsym(RTLD_NEXT, "pthread_create");
    // printf("in uthread create!\n");
    struct uthread *ut = NULL;
    uthread_create(&ut,start_rtn,arg);
    *tidp = (unsigned long)ut;
    return 1;
}

int pthread_join(pthread_t thread, void **retval) {
    printf("in join \n");
    struct uthread* ut = thread;
    uthread_join(ut,NULL);
    // int (*sys_pthread_join)(pthread_t thread, void **retval) = dlsym(RTLD_NEXT, "pthread_join");
    // sys_pthread_join(thread,retval);
    return 0;
}


pthread_t pthread_self(void) {
    return uthread_self();
}


void pthread_exit(void *retval) {
    uthread_exit(NULL);
}


int socket(int domain, int type, int protocol) {
    int ret = uthread_socket(domain,type,protocol);
    return ret;
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    int ret = uthread_connect(sockfd,addr,addrlen);
    return ret;
}


int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen){
    int ret = uthread_accept(sockfd,addr,addrlen);
    return ret;
}



ssize_t	 read(int fd, void *buf, size_t length) {
    ssize_t ret = uthread_read(fd, buf, length);
    return ret;
}


ssize_t write(int fd, const void *buf, size_t length) {
    ssize_t res = uthread_write(fd,buf,length);
    return res;
}

ssize_t recv(int fd, void *buf, size_t length, int flags) {
    return uthread_recv(fd,buf,length,flags);
}
ssize_t recvmsg(int fd, struct msghdr *message, int flags) {
    return uthread_recvmsg(fd,message,flags);
}
ssize_t recvfrom(int fd, void *buf, size_t length, int flags, struct sockaddr *address, socklen_t *address_len) {
    return uthread_recvfrom(fd,buf,length,flags,address,address_len);
}
ssize_t recv_exact(int fd, void *buf, size_t length, int flags) {
    return uthread_recv_exact(fd,buf,length,flags);
}
ssize_t read_exact(int fd, void *buf, size_t length){
    return uthread_read_exact(fd,buf,length);
}

ssize_t send(int fd, const void *buf, size_t length, int flags) {
    return uthread_send(fd,buf,length,flags);
}
ssize_t sendmsg(int fd, const struct msghdr *message, int flags) {
    return uthread_sendmsg(fd,message,flags);
}
ssize_t sendto(int fd, const void *buf, size_t length, int flags, const struct sockaddr *dest_addr, socklen_t dest_len) {
    return uthread_sendto(fd,buf,length,flags,dest_addr,dest_len);
}
ssize_t writev(int fd, struct iovec *iov, int iovcnt){
    return uthread_writev(fd,iov,iovcnt);
}




int enable_hook() {
    return 1;
}