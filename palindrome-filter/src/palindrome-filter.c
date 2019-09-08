#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<fcntl.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<sys/ipc.h>
#include<sys/msg.h>
#include<unistd.h>
#include<sys/wait.h>
#include<errno.h>
#define true 1
#define false 0
#define N 4096
#define MSGSZ 1024
typedef char bool;
typedef enum {REPLY_DATA,REPLY_DATA_STOP,REPLY_ERR} Payload;
typedef struct
{
    long int type;
    Payload payload;
    char txt;
    long int n;
} Messaggio;

char program[N];

//W process

int childhoodW(int msqid,char*dstFile)
{
    Messaggio messaggio;
    FILE*fds;
    if(dstFile)
    {
        if((fds=fopen(dstFile,"w"))==NULL)
        {
            fprintf(stderr,"%s: Can't open file\n",program);
            messaggio.type=3;
            messaggio.payload=REPLY_ERR;
            if((msgsnd(msqid,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1)
            {
                perror("error: msgsnd");
                return -1;
            }
            return -1;
        }
    }
    else
    {
        fds=stdout;
    }
    messaggio.type=3;
    messaggio.payload=REPLY_DATA;
    if((msgsnd(msqid,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1)
    {
        perror("error: msgsnd");
        return -1;
    }
    bool flag=false;
    while(true)
    {
        if((msgrcv(msqid,&messaggio,sizeof(messaggio)-sizeof(long),2,0))==-1)
        {
            fprintf(stderr,"%s: msgrcv\n",program);
            return -1;
        }
        if(messaggio.payload==REPLY_DATA_STOP)
        {
            //fputc(messaggio.txt,fds);
            break;
        }
        if(messaggio.payload==REPLY_ERR)
        {
            return -1;
        }
        if((!dstFile) && (!flag))
        {
            printf("%s: Lista palindrome:\n",program);
            flag=true;
        }
        if(messaggio.payload==REPLY_DATA)
        {
            fputc(messaggio.txt,fds);
        }
    }
    if(dstFile)
    {
        fclose(fds);
    }
    return 0;
}

//R process

int mandaLettera(int msqid,FILE*fds)
{
    Messaggio messaggio;
    while((messaggio.txt=fgetc(fds)))
    {
        if(((fds==stdin) && (messaggio.txt=='\n')) || ((fds!=stdin) && (messaggio.txt==EOF)))
        {
            break;
        }
        messaggio.type=3;
        messaggio.payload=REPLY_DATA;
        if((msgsnd(msqid,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1)
        {
            perror("error: msgsnd");
            return -1;
        }
    }
    messaggio.type=3;
    messaggio.payload=REPLY_DATA_STOP;
    if((msgsnd(msqid,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1)
    {
        perror("error: msgsnd");
        return -1;
    }
    return 0;
}

int childhoodR(int msqid,char*srcFile)
{
    Messaggio messaggio;
    FILE*fds;
    if(srcFile)
    {
        //printf("ciao\n");
        if((access(srcFile,F_OK))==-1)
        {
            fprintf(stderr,"%s: \"%s\" No such file or directory\n",program,srcFile);
            messaggio.type=3;
            messaggio.payload=REPLY_ERR;
            if((msgsnd(msqid,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1)
            {
                perror("error: msgsnd");
                return -1;
            }
            return -1;
        }
        if((access(srcFile,R_OK))==-1)
        {
            fprintf(stderr,"%s: Permission denied\n",program);
            messaggio.type=3;
            messaggio.payload=REPLY_ERR;
            if((msgsnd(msqid,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1)
            {
                perror("error: msgsnd");
                return -1;
            }
            return -1;
        }
        if(!(fds=fopen(srcFile,"r")))
        {
            fprintf(stderr,"%s: Can't open file\n",program);
            messaggio.type=3;
            messaggio.payload=REPLY_ERR;
            if((msgsnd(msqid,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1)
            {
                perror("error: msgsnd");
                return -1;
            }
            return -1;
        }
    }
    else
    {
        fds=stdin;
        printf("%s: Inserisci la parola ",program);
    }
    messaggio.type=3;
    messaggio.payload=REPLY_DATA;
    if((msgsnd(msqid,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1)
    {
        perror("error: msgsnd");
        return -1;
    }
    if((mandaLettera(msqid,fds))==-1)
    {
        fprintf(stderr,"%s: prendiLettera\n",program);
        return -1;
    }
    if(srcFile)
    {
        fclose(fds);
    }
    return 0;
}

//Parent process

char*prendiLettera(int msqid,char*buffer)
{
    Messaggio messaggio;
    while(true)
    {
        if((msgrcv(msqid,&messaggio,sizeof(messaggio)-sizeof(long),3,0))==-1)
        {
            fprintf(stderr,"%s: msgrcv\n",program);
            return NULL;
        }
        if(messaggio.payload==REPLY_DATA_STOP)
        {
            break;
        }
        if(messaggio.payload==REPLY_ERR)
        {
            return NULL;
        }
        sprintf(buffer+strlen(buffer),"%c",messaggio.txt);
    }
    return buffer;
}

bool checkPalindrome(char*buffer)
{
    size_t length=strlen(buffer);
    for(int i=0; i<=length>>1; i++)
    {
        if(buffer[i]!=buffer[length-1-i])
        {
            return false;
        }
    }
    return true;
}

int mandaPalindroma(int msqid,char*palindroma)
{
    Messaggio messaggio;
    for(int i=0; i<=strlen(palindroma)-1; i++)
    {
        messaggio.type=2;
        messaggio.payload=REPLY_DATA;
        messaggio.txt=palindroma[i];
        if((msgsnd(msqid,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1)
        {
            perror("error: msgsnd");
            return -1;
        }
    }
    messaggio.type=2;
    messaggio.payload=REPLY_DATA;
    messaggio.txt='\n';
    if((msgsnd(msqid,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1)
    {
        perror("error: msgsnd");
        return -1;
    }
    return 0;
}

int palindormeFilter(char*srcFile,char*dstFile)
{
    int msqid;
    key_t key=IPC_PRIVATE;
    mode_t mode=S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    if((msqid=msgget(key,IPC_CREAT | IPC_EXCL | mode))==-1)
    {
        fprintf(stderr,"%s: ",program);
        perror("msgget");
        return -1;
    }
    Messaggio messaggio;
    pid_t pid;
    if((pid=fork())==-1)
    {
        fprintf(stderr,"%s: Can't create a new process\n",program);
        return -1;
    }
    if(!pid)
    {
        if((childhoodR(msqid,srcFile))==-1)
        {
            exit(EXIT_FAILURE);
        }
        exit(EXIT_SUCCESS);
    }
    if((msgrcv(msqid,&messaggio,sizeof(messaggio)-sizeof(long),3,0))==-1)
    {
        fprintf(stderr,"%s: msgrcv\n",program);
        return -1;
    }
    if(messaggio.payload==REPLY_ERR)
    {
        return -1;
    }
    if((pid=fork())==-1)
    {
        fprintf(stderr,"%s: Can't create a new process\n",program);
        return -1;
    }
    if(!pid)
    {
        if((childhoodW(msqid,dstFile))==-1)
        {
            exit(EXIT_FAILURE);
        }
        exit(EXIT_SUCCESS);
    }
    if((msgrcv(msqid,&messaggio,sizeof(messaggio)-sizeof(long),3,0))==-1)
    {
        fprintf(stderr,"%s: msgrcv\n",program);
        return -1;
    }
    if(messaggio.payload==REPLY_ERR)
    {
        wait(NULL);
        wait(NULL);
        return -1;
    }
    char*buffer=(char*)malloc(N);
    if(!(buffer=prendiLettera(msqid,buffer)))
    {
        free(buffer);
        wait(NULL);
        return -1;
    }
    for(char*token=strtok(buffer,"\n");token;token=strtok(NULL,"\n")) //#
    {
        if(checkPalindrome(token))
        {
            //printf("P %s\n",token);
            if((mandaPalindroma(msqid,token))==-1)
            {
                fprintf(stderr,"%s: mandaPalindroma\n",program);
                free(buffer);
                wait(NULL);
                return -1;
            }
        }
    }
    messaggio.type=2;
    messaggio.payload=REPLY_DATA_STOP;
    messaggio.txt='\n';
    if((msgsnd(msqid,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1)
    {
        perror("error: msgsnd");
        return -1;
    }
    wait(NULL);
    wait(NULL);
    msgctl(msqid,IPC_RMID,NULL);
    return 0;
}

/*int ciao(char*srcFile,char*dstFile)
{
    if(srcFile)
    {
        printf("TRUE\n");
    }
    if(dstFile)
    {
        printf("TRUE\n");
    }
    return 0;
}*/

int main(int argc,char**argv)
{
    extern int errno;
    strcpy(program,argv[0]);
    if(argc>=4)
    {
        printf("usage: %s [input file] [output file]\n",program);
        exit(EXIT_SUCCESS);
    }
    char*srcFile;
    char*dstFile;
    if(argc>=2)
    {
        srcFile=strdup(argv[1]);
    }
    if(argc==3)
    {
        dstFile=strdup(argv[2]);
    }
    if((palindormeFilter(srcFile,dstFile))==-1)
    {
        if(srcFile)
        {
            free(srcFile);
        }
        if(dstFile)
        {
            free(dstFile);
        }
        exit(EXIT_FAILURE);
    }
    if(srcFile)
    {
        free(srcFile);
    }
    if(dstFile)
    {
        free(dstFile);
    }
    exit(EXIT_SUCCESS);
}
