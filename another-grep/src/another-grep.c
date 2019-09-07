#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<fcntl.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<sys/ipc.h>
#include<sys/msg.h>
#include<unistd.h>
#include<sys/wait.h>
#include<errno.h>
#define true 1
#define false 0
#define N 4096
#define MSGSZ 1024
#define STDIN 0
#define STDOUT 1
#define STDERR 2
typedef int bool;
typedef enum {REPLY_DATA,REPLY_DATA_STOP,REPLY_ERR} Payload;
typedef struct
{
    long int type;
    Payload payload;
    char txt[MSGSZ];
    long int n;
} Messaggio;

char program[N];

//W process

int childhoodW(int msqid)
{
    Messaggio messaggio;
    printf("\n");
    while(true)
    {
        if((msgrcv(msqid,&messaggio,sizeof(messaggio)-sizeof(long),0,0))==-1)
        {
            fprintf(stderr,"%s: msgrcv\n",program);
            return -1;
        }
        if(messaggio.payload==REPLY_DATA_STOP)
        {
            printf("\n");
            break;
        }
        else if(messaggio.payload==REPLY_DATA)
        {
            printf("%s",messaggio.txt);
        }
    }
    return 0;
}

//R process

int childhoodR(int*pipefd,char*file)
{
    close(pipefd[0]);
    if((access(file,F_OK))==-1)
    {
        fprintf(stderr,"%s: \"%s\" No such file or directory\n",program,file);
        return -1;
    }
    if((access(file,R_OK))==-1)
    {
        fprintf(stderr,"%s: Permission denied\n",program);
        return -1;
    }
    FILE*fds;
    char*buffer=(char*)malloc(MSGSZ);
    if((fds=fopen(file,"r"))==NULL)
    {
        fprintf(stderr,"%s: Can't open file\n",program);
        free(buffer);
        return -1;
    }
    while(fgets(buffer,N,fds))
    {
        if((write(pipefd[1],buffer,MSGSZ))==-1)
        {
            fprintf(stderr,"%s: Can't write on pipe\n",program);
            free(buffer);
            return -1;
        }
    }
    fclose(fds);
    close(pipefd[1]);
    /*int fd;
    ssize_t size;
    if((fd=open(file,O_RDONLY))==-1)
    {
        return -1;
    }
    do{
        if((size=read(fd,buffer,N))==-1)
        {
            return -1;
        }
        if((write(pipefd[1],buffer,N))==-1)
        {
            return -1;
        }
    }while(size==N);
    close(fd);
    close(pipefd[1]);*/
    if(buffer)
    {
        free(buffer);
    }
    return 0;
}

//Parent process

int anotherGrep(char*parola,char*file)
{
    int msqid;
    key_t key=IPC_PRIVATE;
    mode_t mode=S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    if((msqid=msgget(key,IPC_CREAT | IPC_EXCL | mode))==-1)
    {
        fprintf(stderr,"%s: msgget\n",program);
        return -1;
    }
    pid_t pid;
    int pipefd[2];
    if((pipe(pipefd))==-1)
    {
        fprintf(stderr,"%s: Can't create pipe\n",program);
        return -1;
    }
    if((pid=fork())==-1)
    {
        fprintf(stderr,"%s: Can't create a new process\n",program);
        return -1;
    }
    if(!pid)
    {
        if((childhoodR(pipefd,file))==-1)
        {
            exit(EXIT_FAILURE);
        }
        exit(EXIT_SUCCESS);
    }
    close(pipefd[1]);
    Messaggio messaggio;
    if((pid=fork())==-1)
    {
        fprintf(stderr,"%s: Can't create a new process\n",program);
        return -1;
    }
    if(!pid)
    {
        if((childhoodW(msqid))==-1)
        {
            exit(EXIT_FAILURE);
        }
        exit(EXIT_SUCCESS);
    }
    ssize_t size;
    do{
        if((size=read(pipefd[0],messaggio.txt,MSGSZ))==-1)
        {
            fprintf(stderr,"%s: Can't read from pipefd\n",program);
        }
        if(strstr(messaggio.txt,parola))
        {
            messaggio.type=1;
            messaggio.payload=REPLY_DATA;
            if((msgsnd(msqid,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1)
            {
                fprintf(stderr,"%s: msgsnd\n",program);
            }
        }
    }while(size);
    messaggio.type=1;
    messaggio.payload=REPLY_DATA_STOP;
    if((msgsnd(msqid,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1)
    {
        fprintf(stderr,"%s: msgsnd\n",program);
    }
    wait(NULL);
    wait(NULL);
    close(pipefd[0]);
    msgctl(msqid,IPC_RMID,NULL);
    return 0;
}

int main(int argc,char**argv)
{
    extern int errno;
    strcpy(program,argv[0]);
    if(argc!=3)
    {
        printf("usage: %s <parola> <file>\n",program);
        exit(EXIT_SUCCESS);
    }
    if((anotherGrep(argv[1],argv[2]))==-1)
    {
        exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
}
