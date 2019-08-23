#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/types.h>
#include<sys/mman.h>
#include<sys/stat.h>
#include<limits.h>
#include<pwd.h>
#include<dirent.h>
#include<libgen.h>
#include<fcntl.h>
#include<unistd.h>
#include<sys/wait.h>
#include<errno.h>
#define true 1
#define false 0
#define STDIN 0
#define STDOUT 1
#define STDERR 2
#define N 4096

//printf("%s\n%ld\n",comando,strlen(comando));

char*usage(char*program,char*arguments)
{
    char*msg=(char*)malloc(N);
    switch(errno)
    {
        case EACCES:
        {
            sprintf(msg,"%s: %s: Permission denied\n",program,arguments);
            return msg;
        }
        case ENOENT:
        {
            sprintf(msg,"%s: %s: No such file or directory\n",program,arguments);
            return msg;
        }
        case EPERM:
        {
            sprintf(msg,"%s: %s: Operation not permitted\n",program,arguments);
            return msg;
        }
        case EIO:
        {
            sprintf(msg,"%s: %s: Input/output error\n",program,arguments);
            return msg;
        }
        case EBADF:
        {
            sprintf(msg,"%s: %s: Bad file descriptor\n",program,arguments);
            return msg;
        }
    }
    if(msg)
    {
        free(msg);
    }
    if(!strcmp(arguments,"--help"))
    {
        printf("\n%s: creata durante le vacanze estive per esercitarmi con un minishell.\nQuesta minishell può gestire gli spazi, ma a differenza delle shell non tramite \\ ma tramite solo l'ausilio delle \" o '.\nBisogna fare molta attenzione mentre si digita una path abbastanza lunga e con la presenza di spazi, e' fortemente consigliato di mettere tutto nelle \" o ' per non creare ambiguita'\n\n(づ｡◕‿‿◕｡)づ Buon Divertimento!! ~(‾▿‾~)\n\n",program);
    }
    return NULL;
}

int calls(char*program,char*comando,size_t length)
{
    mode_t mode=S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    int isBackground=false;
    char*arguments[N];
    int fd;
    int i=0;
    int stds=false;
    pid_t pid;
    const char SQ[]="\'";
    const char DQ[]="\"";
    char*pathIn=NULL,*pathOut=NULL,*pathOutApp=NULL,*pathErr=NULL;
    for(char*token=strtok(comando," "); token; token=strtok(NULL," "))
    {
        if(token[0]==SQ[0])
        {
            token[strlen(token)]=' ';
            token=strtok(token,SQ);
        }
        else if(token[0]==DQ[0])
        {
            token[strlen(token)]=' ';
            token=strtok(token,DQ);
        }
        if((token[0]=='*') || (token[strlen(token)-1]=='*'))
        {
            printf("(☞ﾟヮﾟ)☞ Eh volevi…\nScrivi tutta la path ciccio! ｡^‿^｡\n");
            return 0;
        }
        if(!strcmp(token,">"))
        {
            if(!pathOut)
            {
                if((token=strtok(NULL," "))==NULL)
                {
                    return 0;
                }
                if(token[0]=='~')
                {
                    char*path=(char*)malloc(N);
                    uid_t uid=getuid();
                    struct passwd* pwd=getpwuid(uid);
                    sprintf(path,"%s%s",pwd->pw_dir,token+1);
                    pathOut=strdup(path);
                    free(path);
                }
                else
                {
                    pathOut=strdup(token);
                }
            }
            continue;
        }
        if(!strcmp(token,">>"))
        {
            if(!pathOutApp)
            {
                if((token=strtok(NULL," "))==NULL)
                {
                    return 0;
                }
                if(token[0]=='~')
                {
                    char*path=(char*)malloc(N);
                    uid_t uid=getuid();
                    struct passwd* pwd=getpwuid(uid);
                    sprintf(path,"%s%s",pwd->pw_dir,token+1);
                    pathOut=strdup(path);
                    free(path);
                }
                else
                {
                    pathOutApp=strdup(token);
                }
            }
            continue;
        }
        if(!strcmp(token,"<"))
        {
            if(!pathIn)
            {
                if((token=strtok(NULL," "))==NULL)
                {
                    return 0;
                }
                if(token[0]=='~')
                {
                    char*path=(char*)malloc(N);
                    uid_t uid=getuid();
                    struct passwd* pwd=getpwuid(uid);
                    sprintf(path,"%s%s",pwd->pw_dir,token+1);
                    pathOut=strdup(path);
                    free(path);
                }
                else
                {
                    pathIn=strdup(token);
                }
            }
            continue;
        }
        if(!strcmp(token,"2>"))
        {
            if(!pathErr)
            {
                if((token=strtok(NULL," "))==NULL)
                {
                    return 0;
                }
                if(token[0]=='~')
                {
                    char*path=(char*)malloc(N);
                    uid_t uid=getuid();
                    struct passwd* pwd=getpwuid(uid);
                    sprintf(path,"%s%s",pwd->pw_dir,token+1);
                    pathOut=strdup(path);
                    free(path);
                }
                else
                {
                    pathErr=strdup(token);
                }
            }
            continue;
        }
        if((i>=1) && (!strcmp(token,"&")))
        {
            isBackground=true;
            continue;
        }
        arguments[i]=strdup(token);
        i++;
    }
    arguments[i]=NULL;
    //printf("%s\n%ld\n",comando,strlen(comando));
    if(!strcmp(arguments[0],program))
    {
        fprintf(stderr,"%s: this program it's already run\n",program);
        return 0;
    }
    if(!strcmp(arguments[0],"cd"))
    {
        if(!arguments[1])
        {
            uid_t uid=getuid();
            struct passwd* pwd=getpwuid(uid);
            chdir(pwd->pw_dir);
        }
        else
        {
            if(arguments[1][0]=='~')
            {
                char*path=(char*)malloc(N);
                uid_t uid=getuid();
                struct passwd* pwd=getpwuid(uid);
                sprintf(path,"%s%s",pwd->pw_dir,arguments[1]+1);
                if((chdir(path))==-1)
                {
                    fprintf(stderr,"%s: %s: Not a directory\n", arguments[0], path);
                }
                free(path);
            }
            else if((chdir(arguments[1]))==-1)
            {
                fprintf(stderr,"%s: %s: Not a directory\n", arguments[0], arguments[1]);
            }
        }
        return 0;
    }
    if((pid=fork())==-1)
    {
        perror("can't create new process");
        return -1;
    }
    if(!pid)
    {
        if(pathIn)
        {
            if((access(pathIn,F_OK))==-1)
            {
                char*msg;
                fprintf(stderr,"%s",usage(program,arguments[0]));
                free(msg);
                exit(2);
            }
            if((access(pathIn,R_OK))==-1)
            {
                char*msg;
                fprintf(stderr,"%s",usage(program,arguments[0]));
                free(msg);
                exit(2);
            }
            int fd;
            if((fd=open(pathIn,O_RDONLY))==-1)
            {
                char*msg;
                fprintf(stderr,"%s",usage(program,pathIn));
                free(msg);
                exit(2);
            }
            close(STDIN);
            dup(fd);
        }
        if(pathOut)
        {
            if((fd=open(pathOut, O_WRONLY | O_CREAT | O_TRUNC,mode))==-1)
            {
                char*msg;
                fprintf(stderr,"%s",usage(program,pathOut));
                free(msg);
                exit(2);
            }
            close(STDOUT);
            dup(fd);
        }
        if(pathOutApp)
        {
            if((fd=open(pathOutApp, O_RDWR | O_APPEND | O_CREAT,mode))==-1)
            {
                char*msg;
                fprintf(stderr,"%s",usage(program,pathOutApp));
                free(msg);
                exit(2);
            }
            close(STDOUT);
            dup(fd);
        }
        if(pathErr)
        {
            if((fd=open(pathErr,O_RDWR | O_APPEND | O_CREAT,mode))==-1)
            {
                char*msg;
                fprintf(stderr,"%s",usage(program,pathErr));
                free(msg);
                exit(2);
            }
            close(STDERR);
            dup(fd);
        }
        if((execvp(arguments[0],arguments))==-1)
        {
            fprintf(stderr,"%s: %s: Command not found\n",program,arguments[0]);
            exit(2);
        }
        //close(fd);
        exit(0);
    }
    else
    {
        if(!isBackground)
        {
            wait(NULL);
        }
    }
    if(pathIn)
    {
        //printf("ciao\n");
        free(pathIn);
    }
    if(pathOut)
    {
        //printf("free Output\n");
        free(pathOut);
    }
    if(pathOutApp)
    {
        //printf("ciao\n");
        free(pathOutApp);
    }
    if(pathErr)
    {
        //printf("free Error\n");
        free(pathErr);
    }
    i=0;
    while(arguments[i])
    {
        //printf("%s ",arguments[i]);
        //printf("%d\n",i);
        free(arguments[i]);
        i++;
    }
    return 0;
}

int startShell(char*program)
{
    printf("\n  ___  __   __    __   ____  _  _  ____  __    __   \n / __)(  ) / _\\  /  \\ / ___)/ )( \\(  __)(  )  (  )  \n( (__  )( /    \\(  O )\\___ \\) __ ( ) _) / (_/\\/ (_/\\\n \\___)(__)\\_/\\_/ \\__/ (____/\\_)(_/(____)\\____/\\____/\n(ﾉ◕ヮ◕)ﾉ*:・ﾟ\n\n\n");
    char*comando=(char*)malloc(N);
    char cwd[PATH_MAX];
    size_t length;
    while(true)
    {
        if(!getcwd(cwd,PATH_MAX))
        {
            perror("error: unknown directory");
            if(comando)
            {
                free(comando);
            }
            return -1;
        }
        printf("bash-sh:%s nanoshell$ ", basename(cwd));
        fgets(comando,N,stdin);
        if((length=strlen(comando))==1)
        {
            continue;
        }
        if(comando[length-1]=='\n')
        {
            comando[length-1]='\0';
        }
        if((length=strlen(comando))==0)
        {
            continue;
        }
        if((!strcmp(comando,"quit")) || (!strcmp(comando,"q")) || (!strcmp(comando,"QUIT")) || (!strcmp(comando,"Q")))
        {
            printf("\n ____  _  _  ____ \n(  _ \\( \\/ )(  __)\n ) _ ( )  /  ) _) \n(____/(__/  (____)\n\n");
            break;
        }
        if(!strcmp(comando,"exit"))
        {
            exit(0);
        }
        int c=0;
        if((c=calls(program,comando,length))==-1)
        {
            perror("error: can't call command");
            if(comando)
            {
                free(comando);
            }
            return -1;
        }
        if(c==0)
        {
            continue;
        }
        else
        {
            break;
        }
    }
    if(comando)
    {
        free(comando);
    }
    return 0;
}

int main(int argc, char**argv)
{
    extern int errno;
    if((argc>=2) && (!strcmp(argv[1],"--help")))
    {
        usage(argv[0],argv[1]);
        exit(0);
    }
    if((startShell(argv[0]))==-1)
    {
        perror("error: can't run program");
        exit(1);
    }
    exit(0);
}
