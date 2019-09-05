#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<fcntl.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<sys/ipc.h>
#include<sys/msg.h>
#include<sys/mman.h>
#include<dirent.h>
#include<libgen.h>
#include<unistd.h>
#include<sys/wait.h>
#include<errno.h>
#define true 1
#define false 0
#define N 4096
#define LENGHOFMESSAGES 1024
#define PARENTID 1000
typedef int bool;
typedef enum {CMD_LIST,CMD_SIZE,CMD_SEARCH,CMD_EXIT,REPLY_DATA,REPLY_DATA_STOP,REPLY_ERR} Payload;
typedef struct 
{
	long int type;
	char txt[LENGHOFMESSAGES];
	Payload payload;
	off_t size;
	long int n;
} Messaggio;
char program[N];

//Child process

int list(int msqid,char*directory)
{
	Messaggio messaggio;
	struct stat statBuff;
	DIR*folder=opendir(".");
	struct dirent*info;
	if(folder)
	{
		while((info=readdir(folder)))
		{
			if((strcmp(info->d_name,".")) && (strcmp(info->d_name,"..")))
			{
				if((lstat(info->d_name,&statBuff))==-1)
				{
                    messaggio.type=PARENTID;
                    messaggio.payload=REPLY_ERR;
                    sprintf(messaggio.txt,"%s: list: lstat: %d",program,errno);
                    if((msgsnd(msqid,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1)
                    {
                        perror("error: msgsnd");
                        exit(EXIT_FAILURE);
                    }
                    return -1;
				}
				if((statBuff.st_mode & S_IFMT)==S_IFREG)
				{
					messaggio.type=PARENTID;
					messaggio.payload=REPLY_DATA;
					strcpy(messaggio.txt,info->d_name);
					if((msgsnd(msqid,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1)
					{
                        perror("error: msgsnd");
						return -1;
					}
				}
			}
		}
		messaggio.type=PARENTID;
		messaggio.payload=REPLY_DATA_STOP;
		if((msgsnd(msqid,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1)
		{
            perror("error: msgsnd");
            exit(EXIT_FAILURE);
		}
		closedir(folder);
	}
	return 0;
}

int size(int msqid,char*directory,char*file)
{
	Messaggio messaggio;
	struct stat statBuff;
    if((access(file,F_OK))==-1)
    {
        messaggio.type=PARENTID;
        messaggio.payload=REPLY_ERR;
        sprintf(messaggio.txt,"%s: size: \"%s\" No such file or directory",program,file);
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
        sprintf(messaggio.txt,"%s: size: Permission denied",program);
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
        sprintf(messaggio.txt,"%s: size: lstat: %d",program,errno);
        if((msgsnd(msqid,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1)
        {
            perror("error: msgsnd");
            exit(EXIT_FAILURE);
        }
        return -1;
    }
    messaggio.type=PARENTID;
    messaggio.payload=REPLY_DATA;
    messaggio.size=statBuff.st_size;
    if((msgsnd(msqid,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1)
    {
        perror("error: msgsnd");
        exit(EXIT_FAILURE);
    }
	return 0;
}

int occurences(char*haystack,char*needle)
{
	int occurences=0;
	while((haystack=strstr(haystack,needle)))
	{
		haystack++;
		occurences++;
	}
	return occurences;
}

int search(int msqid,char*directory,char*file,char*needle)
{
	Messaggio messaggio;
	struct stat statBuff;
    int fd;
    char*haystack;
    if((access(file,F_OK))==-1)
    {
        messaggio.type=PARENTID;
        messaggio.payload=REPLY_ERR;
        sprintf(messaggio.txt,"%s: search: \"%s\" No such file or directory",program,file);
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
        sprintf(messaggio.txt,"%s: search: Permission denied",program);
        if((msgsnd(msqid,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1)
        {
            perror("error: msgsnd");
            exit(EXIT_FAILURE);
        }
        return -1;
    }
    if((fd=open(file,O_RDONLY))==-1)
    {
        messaggio.type=PARENTID;
        messaggio.payload=REPLY_ERR;
        sprintf(messaggio.txt,"%s: search: Can't read file",program);
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
        sprintf(messaggio.txt,"%s: search: lstat",program);
        if((msgsnd(msqid,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1)
        {
            perror("error: msgsnd");
            exit(EXIT_FAILURE);
        }
        return -1;
    }
    if((haystack=(char*)mmap(NULL,statBuff.st_size,PROT_READ,MAP_SHARED,fd,0))==MAP_FAILED)
    {
        messaggio.type=PARENTID;
        messaggio.payload=REPLY_ERR;
        sprintf(messaggio.txt,"%s: search: mmap",program);
        if((msgsnd(msqid,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1)
        {
            perror("error: msgsnd");
            exit(EXIT_FAILURE);
        }
        return -1;
    }
    messaggio.type=PARENTID;
    messaggio.payload=REPLY_DATA;
    messaggio.n=occurences(haystack,needle);
    if((msgsnd(msqid,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1)
    {
        perror("error: msgsnd");
        exit(EXIT_FAILURE);
    }
    munmap(haystack,statBuff.st_size);
    close(fd);
	return 0;
}

int childhood(int msqid,char*directory,int n)
{
	Messaggio messaggio;
	if((access(directory,F_OK))==-1)
	{
        messaggio.type=PARENTID;
        messaggio.payload=REPLY_ERR;
        sprintf(messaggio.txt,"Figlio n. %d directory: \"%s\" No such file or directory",n,directory);
        if((msgsnd(msqid,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1)
        {
            perror("error: msgsnd");
            exit(EXIT_FAILURE);
        }
        return -1;
	}
	if((access(directory,R_OK))==-1)
	{
        messaggio.type=PARENTID;
        messaggio.payload=REPLY_ERR;
        sprintf(messaggio.txt,"Figlio n. %d directory: \"%s\" Permission denied",n,directory);
        if((msgsnd(msqid,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1)
        {
            perror("error: msgsnd");
            exit(EXIT_FAILURE);
        }
        return -1;
	}
	chdir(directory);
    messaggio.type=PARENTID;
    messaggio.payload=REPLY_DATA;
    sprintf(messaggio.txt,"Figlio n. %d directory: \"%s\" PRONTO!",n,directory);
    if((msgsnd(msqid,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1)
    {
        perror("error: msgsnd");
        exit(EXIT_FAILURE);
    }
	while(true)
	{
		if((msgrcv(msqid,&messaggio,sizeof(messaggio)-sizeof(long),n,0))==-1)
		{
			fprintf(stderr,"%s: msgrcv\n",program);
			return -1;
		}
		if(messaggio.payload==CMD_EXIT)
		{
			break;
		}
		else if(messaggio.payload==CMD_LIST)
		{
            list(msqid,directory);
		}
		else if(messaggio.payload==CMD_SIZE)
		{
            size(msqid,directory,messaggio.txt);
		}
		else if(messaggio.payload==CMD_SEARCH)
		{
			char*fileName=strdup(messaggio.txt);
			if((msgrcv(msqid,&messaggio,sizeof(messaggio)-sizeof(long),n,0))==-1)
			{
				fprintf(stderr,"%s: msgrcv\n",program);
				free(fileName);
				return -1;
			}
            search(msqid,directory,fileName,messaggio.txt);
			if(fileName)
			{
				free(fileName);
			}
		}
	}
	return 0;
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------
//Parent process

int comando(int msqid,bool*isAlive,int nDir)
{
	Messaggio messaggio;
	char*comando=(char*)malloc(N);
	size_t length;
	char*arguments[N];
	const char SQ[]="\'";
	const char DQ[]="\"";
	while(true)
	{
		printf("%s: ",program);
		fgets(comando,N,stdin);
		if((length=strlen(comando))==1)
		{
			continue;
		}
		else if(comando[length-1]=='\n')
		{
			comando[length-1]='\0';
		}
		if((!strcmp(comando,"quit")) || (!strcmp(comando,"QUIT")) || (!strcmp(comando,"q")) || (!strcmp(comando,"Q")))
		{
			break;
		}
		int i=0;
		for(char*token=strtok(comando," ");token;token=strtok(NULL," "))
		{
			if(token[0]==SQ[0])
			{
				token[strlen(token)]=' ';
				token=strtok(token,SQ);
			}
			if(token[0]==DQ[0])
			{
				token[strlen(token)]=' ';
				token=strtok(token,DQ);
			}
			arguments[i]=strdup(token);
			i++;
		}
		arguments[i]=NULL;
		if(!strcmp(arguments[0],"list"))
		{
			if(i==2)
			{
				int n=atoi(arguments[1]);
                if(((n<=0) || (n>=nDir)) || (!isAlive[n]))
                {
                    printf("%s: syntax error\n",program);
                    continue;
                }
				messaggio.type=n;
				messaggio.payload=CMD_LIST;
				if((msgsnd(msqid,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1)
				{
					fprintf(stderr,"%s: msgsnd\n",program);
					return -1;
				}
				while(true)
				{
					if((msgrcv(msqid,&messaggio,sizeof(messaggio)-sizeof(long),PARENTID,0))==-1)
					{
						fprintf(stderr,"%s: msgrcv\n",program);
						return -1;
					}
                    if(messaggio.payload==REPLY_ERR)
                    {
                        printf("%s",messaggio.txt);
                        break;
                    }
					if(messaggio.payload==REPLY_DATA_STOP)
					{
						break;
					}
                    if(messaggio.payload==REPLY_DATA)
                    {
                        printf("\t%s\n",messaggio.txt);
                    }
				}
			}
			else
			{
				printf("%s: syntax error\n",program);
			}
		}
		else if(!strcmp(arguments[0],"size"))
		{
			if(i==3)
			{
				int n=atoi(arguments[1]);
                if(((n<=0) || (n>=nDir)) || (!isAlive[n]))
                {
                    printf("%s: syntax error\n",program);
                    continue;
                }
				messaggio.type=n;
				messaggio.payload=CMD_SIZE;
				strcpy(messaggio.txt,arguments[2]);
				if((msgsnd(msqid,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1)
				{
					fprintf(stderr,"%s: msgsnd\n",program);
					return -1;
				}
				if((msgrcv(msqid,&messaggio,sizeof(messaggio)-sizeof(long),PARENTID,0))==-1)
				{
					fprintf(stderr,"%s: msgrcv\n",program);
					return -1;
				}
                if(messaggio.payload==REPLY_ERR)
                {
                    printf("%s\n",messaggio.txt);
                }
                else
                {
                    printf("Il file e' grande %lldB\n",messaggio.size);
                }
			}
			else
			{
				printf("%s: syntax error\n",program);
			}
		}
		else if(!strcmp(arguments[0],"search"))
		{
			if(i==4)
			{
				int n=atoi(arguments[1]);
                if(((n<=0) || (n>=nDir)) || (!isAlive[n]))
                {
                    printf("%s: syntax error\n",program);
                    continue;
                }
				messaggio.type=n;
				messaggio.payload=CMD_SEARCH;
				strcpy(messaggio.txt,arguments[2]);
				if((msgsnd(msqid,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1)
				{
					fprintf(stderr,"%s: msgsnd\n",program);
					return -1;
				}
				messaggio.type=n;
				messaggio.payload=CMD_SEARCH;
				strcpy(messaggio.txt,arguments[3]);
				if((msgsnd(msqid,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1)
				{
                    perror("error: msgsnd");
					return -1;
				}
				if((msgrcv(msqid,&messaggio,sizeof(messaggio)-sizeof(long),PARENTID,0))==-1)
				{
					fprintf(stderr,"%s: msgrcv\n",program);
					return -1;
				}
                if(messaggio.payload==REPLY_ERR)
                {
                    printf("%s\n",messaggio.txt);
                }
                else
                {
                    if(messaggio.n==1)
                    {
                        printf("La parola \"%s\" e' presente %ld volta\n",arguments[3],messaggio.n);
                    }
                    else
                    {
                        printf("La parola \"%s\" e' presente %ld volte\n",arguments[3],messaggio.n);
                    }
                }
			}
			else
			{
				printf("%s: syntax error\n",program);
			}
		}
		i=0;
		while(arguments[i])
		{
			free(arguments[i]);
			i++;
		}
		sleep(1);
	}
	if(comando)
	{
		free(comando);
	}
	return 0;
}

int fileShell(int nDir,char**dir)
{
	int msqid;
	key_t key=IPC_PRIVATE;
	mode_t mode=S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
	if((msqid=msgget(key,IPC_CREAT | IPC_EXCL | mode))==-1)
	{
		fprintf(stderr,"%s: msgget\n",program);
		return -1;
	}
    Messaggio messaggio;
	int i;
	pid_t pid;
    bool*isAlive=(bool*)malloc((nDir-1)*sizeof(bool));
    isAlive[0]=false;
    int c=0;
	for(i=1; i<=nDir-1; i++)
	{
		if((pid=fork())==-1)
		{
			fprintf(stderr,"%s: Can't create a new process\n",program);
			return -1;
		}
		if(!pid)
		{
			if((childhood(msqid,dir[i],i))==-1)
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
            printf("%s\n",messaggio.txt);
            c++;
            isAlive[i]=true;
        }
        else
        {
            printf("%s\n",messaggio.txt);
            isAlive[i]=false;
        }
	}
    if(c)
    {
        sleep(1);
        if((comando(msqid,isAlive,nDir))==-1)
        {
            fprintf(stderr,"%s: comando\n",program);
            return -1;
        }
        for(i=1; i<=nDir-1; i++)
        {
            messaggio.type=i;
            messaggio.payload=CMD_EXIT;
            if((msgsnd(msqid,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1)
            {
                fprintf(stderr,"%s: msgsnd\n",program);
                return -1;
            }
        }
        
        for(i=1; i<=nDir-1; i++)
        {
            wait(NULL);
        }
    }
    if(isAlive)
    {
        free(isAlive);
    }
	msgctl(msqid,IPC_RMID,NULL);
	return 0;
}

int main(int argc,char**argv)
{
	extern int errno;
	strcpy(program,argv[0]);
	if(argc==1)
	{
		printf("usage: %s <directory1> <directory2> ...\n",program);
		exit(EXIT_SUCCESS);
	}
	if((fileShell(argc,argv))==-1)
	{
		exit(EXIT_FAILURE);
	}
	exit(EXIT_SUCCESS);
}
