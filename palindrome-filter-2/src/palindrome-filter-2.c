#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<fcntl.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<sys/mman.h>
#include<unistd.h>
#include<sys/wait.h>
#include<errno.h>
#define true 1
#define false 0
#define N 4096
typedef char bool;

char program[N];

//W process

int childhoodW(int*pPtoW)
{
    close(pPtoW[1]);
    FILE*fds;
    char*str=(char*)malloc(N);
    bool anyPalindrome=false;
    if((fds=fdopen(pPtoW[0],"r"))==NULL)
    {
        fprintf(stderr,"%s: W Can't stream pipe:",program);
        perror("");
        return -1;
    }
    while(fgets(str,N,fds))
    {
        if(!anyPalindrome)
        {
            printf("%s: Lista palindrome:\n",program);
            anyPalindrome=true;
        }
        printf("%s",str);
    }
    if(!anyPalindrome)
    {
        printf("%s: Nessuna parola e' palindroma\n",program);
    }
    //printf("\n");
    free(str);
    fclose(fds);
    close(pPtoW[0]);
    return 0;
}

//R process

int childhoodR(int*pRtoP,char*srcFile)
{
    close(pRtoP[0]);
    int fd;
    FILE*fds;
    char*buffer;
    struct stat statBuff;
    bool flag=false;
    if(srcFile)
    {
        if((access(srcFile,F_OK))==-1)
        {
            fprintf(stderr,"%s: \"%s\" No such file or directory\n",program,srcFile);
            return -1;
        }
        if((access(srcFile,R_OK))==-1)
        {
            fprintf(stderr,"%s: Permission denied\n",program);
            return -1;
        }
        if((fd=open(srcFile,O_RDONLY))==-1)
        {
            fprintf(stderr,"%s: Can't open file\n",program);
            return -1;
        }
        if((lstat(srcFile,&statBuff))==-1)
        {
            fprintf(stderr,"%s: ",program);
            perror("lstat");
            return -1;
        }
        if((buffer=(char*)mmap(NULL,statBuff.st_size,PROT_READ,MAP_SHARED,fd,0))==MAP_FAILED)
        {
            fprintf(stderr,"%s: ",program);
            perror("mmap");
            return -1;
        }
        flag=true;
    }
    else
    {
        buffer=(char*)malloc(N);
        size_t length;
        while(true)
        {
            printf("%s: Inserisci parola ",program);
            fgets(buffer,N,stdin);
            if((length=strlen(buffer))==1)
            {
                continue;
            }
            /*else if(buffer[length-1]=='\n')
            {
                buffer[length-1]='\0';
            }*/
            break;
        }
    }
    if((fds=fdopen(pRtoP[1],"w"))==NULL)
    {
        fprintf(stderr,"%s: R Can't stream pipe:",program);
        perror("");
        return -1;
    }
    fprintf(fds,"%s",buffer);
    if(flag)
    {
        close(fd);
        munmap(buffer,statBuff.st_size);
    }
    else
    {
        free(buffer);
    }
    fclose(fds);
    close(pRtoP[1]);
    return 0;
}

//Parent process

bool checkPalindroma(char*parola)
{
    for(int i=0; i<=strlen(parola)-1; i++)
    {
        //printf("%s",parola);
        if(parola[i]!=parola[strlen(parola)-1-i])
        {
            return false;
        }
    }
    return true;
}

int palindromeFilter2(char*srcFile)
{
    int pRtoP[2], pPtoW[2];
    if((pipe(pRtoP))==-1)
    {
        fprintf(stderr,"%s: ",program);
        perror("pipe:");
        return -1;
    }
    if((pipe(pPtoW))==-1)
    {
        fprintf(stderr,"%s: ",program);
        perror("pipe");
        return -1;
    }
    pid_t pid;
    if((pid=fork())==-1)
    {
        fprintf(stderr,"%s: ",program);
        perror("Can't create a new process");
        wait(NULL);
        return -1;
    }
    if(!pid)
    {
        if((childhoodR(pRtoP,srcFile))==-1)
        {
            exit(EXIT_FAILURE);
        }
        exit(EXIT_SUCCESS);
    }
    close(pRtoP[1]);
    wait(NULL);
    if((pid=fork())==-1)
    {
        fprintf(stderr,"%s: ",program);
        perror("Can't create a new process");
        return -1;
    }
    if(!pid)
    {
        if((childhoodW(pPtoW))==-1)
        {
            exit(EXIT_FAILURE);
        }
        exit(EXIT_SUCCESS);
    }
    close(pPtoW[0]);
    FILE*fdsin;
    FILE*fdsout;
    if((fdsin=fdopen(pRtoP[0],"r"))==NULL)
    {
        fprintf(stderr,"%s: P Can't stream pipe:",program);
        perror("");
        wait(NULL);
        return -1;
    }
    if((fdsout=fdopen(pPtoW[1],"w"))==NULL)
    {
        fprintf(stderr,"%s: P Can't stream pipe:",program);
        perror("");
        wait(NULL);
        return -1;
    }
    char*parola=(char*)malloc(N);
    bool flag=false;
    while(fgets(parola,N,fdsin))
    {
        parola[strlen(parola)-1]='\0';
        if(checkPalindroma(parola))
        {
            parola[strlen(parola)]='\n';
            fprintf(fdsout,"%s",parola);
        }
        /*flag=false;
        for(int i=0; i<=strlen(parola)-2; i++)
        {
            //printf("%s",parola);
            if(parola[i]!=parola[strlen(parola)-2-i])
            {
                flag=false;
            }
        }
        if(flag)
        {
            fprintf(fdsout,"%s",parola);
        }*/
    }
    fclose(fdsin);
    fclose(fdsout);
    if(parola)
    {
        free(parola);
    }
    wait(NULL);
    return 0;
}

int main(int argc,char**argv)
{
    extern int errno;
    strcpy(program,argv[0]);
    char*srcFile;
    if(argc>=3)
    {
        printf("usage: %s <input file>\n",program);
        exit(EXIT_SUCCESS);
    }
    if(argc==2)
    {
        srcFile=strdup(argv[1]);
    }
    if((palindromeFilter2(srcFile))==-1)
    {
        if(srcFile)
        {
            free(srcFile);
        }
        exit(EXIT_FAILURE);
    }
    if(srcFile)
    {
        free(srcFile);
    }
    exit(EXIT_SUCCESS);
}
