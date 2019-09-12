#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<fcntl.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<sys/ipc.h>
#include<sys/shm.h>
#include<sys/sem.h>
#include<dirent.h>
#include<libgen.h>
#include<unistd.h>
#include<sys/wait.h>
#include<errno.h>
#define true 1
#define false 0
#define N 4096
#define S_MUTEX 0
#define S_FILE_COND 1
#define S_DIR_COND 2
typedef char bool;
typedef enum {REPLY_DATA,REPLY_ERR,REPLY_STOP} Payload;
typedef struct
{
	Payload payload;
	off_t size;
	char info[N];
	long int n;
} Messaggio;
char program[N];

int WAIT(int semid,int cond)
{
	struct sembuf operazioni[1]={{cond,-1,0}};
	return semop(semid,operazioni,1);
}

int SIGNAL(int semid,int cond)
{
	struct sembuf operazioni[1]={{cond,+1,0}};
	return semop(semid,operazioni,1);
}

//File-Consumer process

int fileConsumer(int shmid,int semid)
{
	Messaggio*messaggio;
	if((messaggio=(Messaggio*)shmat(shmid,NULL,0))==(Messaggio*)-1)
	{
		perror("error: shmat");
		return -1;
	}
	while(true)
	{
		WAIT(semid,S_FILE_COND);
		if(messaggio->payload==REPLY_DATA)
		{
			printf("%s [file di %ld bytes]\n",messaggio->info,messaggio->size);
			SIGNAL(semid,S_MUTEX);
		}
		else
		{
			break;
		}
	}
	return 0;
}

//Dir-Consumer process

int dirConsumer(int shmid,int semid)
{
	Messaggio*messaggio;
	if((messaggio=(Messaggio*)shmat(shmid,NULL,0))==(Messaggio*)-1)
	{
		perror("error: shmat");
		return -1;
	}
	while(true)
	{
		WAIT(semid,S_DIR_COND);
		if(messaggio->payload==REPLY_DATA)
		{
			printf("%s [directory]\n",messaggio->info);
			SIGNAL(semid,S_MUTEX);
		}
		else
		{
			break;
		}
	}
	return 0;
}

//Reader processes

int reader(int shmid,int semid,char*directory)
{
	chdir(directory);
	Messaggio*messaggio;
	if((messaggio=(Messaggio*)shmat(shmid,NULL,0))==(Messaggio*)-1)
	{
		perror("error: shmat");
		return -1;
	}
	struct stat statBuff;
	DIR*folder=opendir(".");
	struct dirent*info;
	if(folder)
	{
		while(info=readdir(folder))
		{
			WAIT(semid,S_MUTEX);
			if((strcmp(info->d_name,"..")) && (strcmp(info->d_name,".")))
			{
				//printf("\n\t%s\n",info->d_name);
				if((lstat(info->d_name,&statBuff))==-1)
				{
					perror("error: lstat");
					SIGNAL(semid,S_MUTEX);
					break;
				}
				if((statBuff.st_mode & S_IFMT)==S_IFREG)
				{
					messaggio->size=statBuff.st_size;
					messaggio->payload=REPLY_DATA;
					sprintf(messaggio->info,"%s/%s",directory,info->d_name);
					SIGNAL(semid,S_FILE_COND);
					WAIT(semid,S_MUTEX);
				}
				else if((statBuff.st_mode & S_IFMT)==S_IFDIR)
				{
					messaggio->payload=REPLY_DATA;
					sprintf(messaggio->info,"%s/%s",directory,info->d_name);
					SIGNAL(semid,S_DIR_COND);
					WAIT(semid,S_MUTEX);
				}
			}
			SIGNAL(semid,S_MUTEX);
		}
	}
	WAIT(semid,S_MUTEX);
	if(messaggio->n>=2)
	{
		messaggio->n--;
		//sleep(1);
	}
	else if(messaggio->n<=1)
	{
		messaggio->payload=REPLY_STOP;
		SIGNAL(semid,S_DIR_COND);
		SIGNAL(semid,S_FILE_COND);
	}
	SIGNAL(semid,S_MUTEX);
	return 0;
}

//Parent process

int listDirs(int n,char**dirs)
{
	int shmid,semid;
	key_t key=IPC_PRIVATE;
	mode_t mode=S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
	if((shmid=shmget(key,sizeof(Messaggio),IPC_CREAT | IPC_EXCL | mode))==-1)
	{
		perror("error: shmget");
		return -1;
	}
	if((semid=semget(key,3,IPC_CREAT | IPC_EXCL | mode))==-1)
	{
		perror("error: semget");
		return -1;
	}
	semctl(semid,S_MUTEX,SETVAL,1);
	semctl(semid,S_FILE_COND,SETVAL,0);
	semctl(semid,S_DIR_COND,SETVAL,0);
	pid_t pid;
	//File-Consumer process
	if((pid=fork())==-1)
	{
		fprintf(stderr,"%s: Can't create a new process: ",program);
		perror("");
		return -1;
	}
	if(!pid)
	{
		if((fileConsumer(shmid,semid))==-1)
		{
			shmctl(shmid,IPC_RMID,NULL);
			semctl(semid,0,IPC_RMID,0);
			exit(EXIT_FAILURE);
		}
		exit(EXIT_SUCCESS);
	}
	//Dir-Consumer process
	if((pid=fork())==-1)
	{
		fprintf(stderr,"%s: Can't create a new process: ",program);
		perror("");
		return -1;
	}
	if(!pid)
	{
		if((dirConsumer(shmid,semid))==-1)
		{
			shmctl(shmid,IPC_RMID,NULL);
			semctl(semid,0,IPC_RMID,0);
			exit(EXIT_FAILURE);
		}
		exit(EXIT_SUCCESS);
	}
	//Readers processes
	Messaggio*messaggio;
	if((messaggio=(Messaggio*)shmat(shmid,NULL,0))==(Messaggio*)-1)
	{
		perror("error: shmat");
		return -1;
	}
	messaggio->n=n-1;
	for(int i=1; i<=n-1; i++)
	{
		if((pid=fork())==-1)
		{
			fprintf(stderr,"%s: Can't create a new process: ",program);
			perror("");
			return -1;
		}
		if(!pid)
		{
			if((reader(shmid,semid,dirs[i]))==-1)
			{
				shmctl(shmid,IPC_RMID,NULL);
				semctl(semid,0,IPC_RMID,0);
				exit(EXIT_FAILURE);
			}
			exit(EXIT_SUCCESS);
		}
	}
	for(int i=1; i<=n-1; i++)
	{
		wait(NULL);
	}
	shmctl(shmid,IPC_RMID,NULL);
	semctl(semid,0,IPC_RMID,0);
	return 0;
}

int main(int argc,char**argv)
{
	extern int errno;
	strcpy(program,argv[0]);
	if(argc==1)
	{
		printf("usage: %s  <dir1> <dir2> <...>\n",program);
	}
	else if((listDirs(argc,argv))==-1)
	{
		exit(EXIT_FAILURE);
	}
	exit(EXIT_SUCCESS);
}