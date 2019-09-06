#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<fcntl.h>
#include<ctype.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<sys/mman.h>
#include<sys/ipc.h>
#include<sys/msg.h>
#include<libgen.h>
#include<unistd.h>
#include<sys/wait.h>
#define true 1
#define false 0
#define N 4096
#define LENGTHOFMESSAGES 1024
#define PARENTID 1000
typedef int bool;
typedef enum {REPLY_DATA,REPLY_ERR} Payload;
typedef struct
{
    long int type;
    char txt[LENGTHOFMESSAGES];
    Payload payload;
    long int n[26];
} Messaggio;
char program[N];

//Child process

int singleAlphaCalc(int msqid,char*file,char*s)
{
    Messaggio messaggio;
    for(int i=0; i<=25; i++)
    {
        messaggio.n[i]=0;
    }
    for(int i=0; i<=strlen(s)-1; i++)
    {
        if(('a'<=s[i]) && (s[i]<='z'))
        {
            messaggio.n[s[i]-'a']++;
        }
        else if(('A'<=s[i]) && (s[i]<='Z'))
        {
            messaggio.n[s[i]-'A']++;
        }
    }
    messaggio.type=PARENTID;
    messaggio.payload=REPLY_DATA;
    sprintf(messaggio.txt,"\"%s\"",basename(file));
    if((msgsnd(msqid,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1)
    {
        perror("error: msgsnd");
        return -1;
    }
    return 0;
}

int childhood(int msqid,char*file,int n)
{
    Messaggio messaggio;
    if((access(file,F_OK))==-1)
    {
        messaggio.type=PARENTID;
        messaggio.payload=REPLY_ERR;
        sprintf(messaggio.txt,"%s: \"%s\" No such file or directory",program,file);
        if((msgsnd(msqid,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1)
        {
            perror("error: msgsnd");
            exit(EXIT_FAILURE);
        }
        return -1;
    }
    if((access(file,R_OK))==-1)
    {
        messaggio.type=PARENTID;
        messaggio.payload=REPLY_ERR;
        sprintf(messaggio.txt,"%s: Permission denied",program);
        if((msgsnd(msqid,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1)
        {
            perror("error: msgsnd");
            exit(EXIT_FAILURE);
        }
        return -1;
    }
    int fd;
    struct stat statBuff;
    if((fd=open(file,O_RDONLY))==-1)
    {
        messaggio.type=PARENTID;
        messaggio.payload=REPLY_ERR;
        sprintf(messaggio.txt,"%s: Can't read file",program);
        if((msgsnd(msqid,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1)
        {
            perror("error: msgsnd");
            exit(EXIT_FAILURE);
        }
        return -1;
    }
    if((lstat(file,&statBuff))==-1)
    {
        messaggio.type=PARENTID;
        messaggio.payload=REPLY_ERR;
        sprintf(messaggio.txt,"%s: lstat",program);
        if((msgsnd(msqid,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1)
        {
            perror("error: msgsnd");
            exit(EXIT_FAILURE);
        }
        return -1;
    }
    char*s;
    if((s=(char*)mmap(NULL,statBuff.st_size,PROT_READ,MAP_SHARED,fd,0))==MAP_FAILED)
    {
        messaggio.type=PARENTID;
        messaggio.payload=REPLY_ERR;
        sprintf(messaggio.txt,"%s: Invalid argument",program);
        if((msgsnd(msqid,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1)
        {
            perror("error: msgsnd");
            exit(EXIT_FAILURE);
        }
        return -1;
    }
    messaggio.type=PARENTID;
    messaggio.payload=REPLY_DATA;
    sprintf(messaggio.txt,"Figlio n. %d file: \"%s\" PRONTO",n,file);
    if((msgsnd(msqid,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1)
    {
        perror("error: msgsnd1");
        exit(EXIT_FAILURE);
    }
    singleAlphaCalc(msqid,file,s);
    munmap(s,statBuff.st_size);
    close(fd);
    return 0;
}

//--------------------------------------------------------------------------------------------------------------------------------------------
//Parent process


int indexMax(int*lettere)
{
    int c=0;
    int s=lettere[0];
    for(int i=0; i<=25; i++)
    {
        if(s<=lettere[i]-1)
        {
            s=lettere[i];
            c=i;
        }
    }
    return c;
}

int alphaCalc(int msqid,int nFiles,bool*isAlive)
{
    Messaggio messaggio;
    int*lettere=(int*)malloc((26)*sizeof(int));
    for(int i=0; i<=25; i++)
    {
        lettere[i]=0;
    }
    for(int i=1;i<=nFiles-1;i++)
    {
        if(!isAlive[i])
        {
            continue;
        }
        if((msgrcv(msqid,&messaggio,sizeof(messaggio)-sizeof(long),PARENTID,0))==-1)
        {
            fprintf(stderr,"%s: msgrcv\n",program);
            free(lettere);
            return -1;
        }
        printf("%s\n",messaggio.txt);
        for(int j=0,lettera=97; j<=25; j++,lettera++)
        {
            printf("%c:%ld ",(char)lettera,messaggio.n[j]);
            lettere[j]+=messaggio.n[j];
        }
        printf("\n\n");
    }
    for(int i=0,lettera=97; i<=25; i++,lettera++)
    {
        printf("%c:%d ",(char)lettera,lettere[i]);
    }
    printf("\n");
    int lettera=indexMax(lettere)+97;
    if((lettera==97) && (lettere[0]==0))
    {
        printf("nessuna lettera e' stata utilizzata\n");
    }
    else
    {
        printf("lettera piu' utilizzata: \'%c\'\n",lettera);
    }
    if(lettere)
    {
        free(lettere);
    }
    return 0;
}

int alphaStats(int nFiles,char**directories)
{
    int msqid;
    key_t key=IPC_PRIVATE;
    mode_t mode=S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    if((msqid=msgget(key,IPC_CREAT | IPC_EXCL | mode))==-1)
    {
        fprintf(stderr,"%s: msgget\n",program);
        return -1;
    }
    bool*isAlive=(bool*)malloc((nFiles-1)*sizeof(bool));
    isAlive[0]=false;
    bool anyAlive=false;
    Messaggio messaggio;
    int status=0;
    pid_t pid;
    for(int i=1; i<=nFiles-1; i++)
    {
        if((pid=fork())==-1)
        {
            fprintf(stderr,"%s: Can't create a new process\n",program);
            return -1;
        }
        if(!pid)
        {
            if((childhood(msqid,directories[i],i))==-1)
            {
                exit(EXIT_FAILURE);
            }
            exit(EXIT_SUCCESS);
        }
        if((msgrcv(msqid,&messaggio,sizeof(messaggio)-sizeof(long),PARENTID,0))==-1)
        {
            fprintf(stderr,"%s: msgrcv\n",program);
            return -1;
        }
        if(messaggio.payload==REPLY_DATA)
        {
            anyAlive=true;
            isAlive[i]=true;
            printf("%s\n",messaggio.txt);
        }
        else
        {
            isAlive[i]=false;
            printf("%s\n",messaggio.txt);
        }
    }
    printf("\n");
    if(anyAlive)
    {
        if((status=alphaCalc(msqid,nFiles,isAlive))==-1)
        {
            fprintf(stderr,"%s: AlphaCalc\n",program);
        }
        for(int i=1; i<=nFiles-1; i++)
        {
            wait(NULL);
        }
    }
    if(isAlive)
    {
        free(isAlive);
    }
    msgctl(msqid,IPC_RMID,NULL);
    return status;
}

int main(int argc,char**argv)
{
    extern int errno;
    strcpy(program,argv[0]);
    if(argc==1)
    {
        printf("usage: %s <file1> <file2> …\n",program);
        exit(EXIT_SUCCESS);
    }
    if((alphaStats(argc,argv))==-1)
    {
        exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
}
