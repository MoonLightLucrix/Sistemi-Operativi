#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<fcntl.h>
#include<dirent.h>
#include<libgen.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<sys/ipc.h>
#include<sys/shm.h>
#include<sys/sem.h>
#include<unistd.h>
#include<sys/wait.h>
#include<errno.h>
#define true 1
#define false 0
#define N 4096
#define MAX_PATH_LEN 2048
#define S_MUTEX 0
#define S_COND_1 1
#define S_COND_2 2
typedef char bool;
typedef enum {REPLY_DATA,REPLY_ERR,REPLY_STOP} Payload;
typedef struct 
{
	blkcnt_t blocco;
	Payload payload;
	char path[MAX_PATH_LEN];
	long int n;
	long int np;
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

//Scanner porcesses

int scanner(int shmid,int semid,char*directory,int n)
{
	Messaggio*messaggio;
	if((messaggio=(Messaggio*)shmat(shmid,NULL,0))==(Messaggio*)-1)
	{
		fprintf(stderr,"%s: ",program);
		perror("shmat");
		return -1;
	}
	chdir(directory);
	DIR*folder=opendir(".");
	struct dirent*info;
	if(folder)
	{
		while(info=readdir(folder))
		{
			WAIT(semid,S_MUTEX);
			//printf("\n\t%s/%s\n",directory,info->d_name);
			if((strcmp(info->d_name,".") && (strcmp(info->d_name,".."))))
			{
				if(info->d_type==DT_REG)
				{
					if(directory[strlen(directory)-1]=='/')
					{
						directory[strlen(directory)-1]='\0';
					}
					sprintf(messaggio->path,"%s/%s",directory,info->d_name);
					messaggio->n=n;
					messaggio->payload=REPLY_DATA;
					SIGNAL(semid,S_COND_1);
				}
				else if(info->d_type==DT_DIR)
				{
					char dir[MAX_PATH_LEN];
					if(directory[strlen(directory)-1]=='/')
					{
						directory[strlen(directory)-1]='\0';
					}
					sprintf(dir,"%s/%s",directory,info->d_name);
					messaggio->np++;
					SIGNAL(semid,S_MUTEX);
					if((scanner(shmid,semid,dir,n))==-1)
					{
						return -1;
					}
				}
				else
				{
					SIGNAL(semid,S_MUTEX);
				}
			}
			else
			{
				SIGNAL(semid,S_MUTEX);
			}
		}
	}
	WAIT(semid,S_MUTEX);
	if(messaggio->np>=2)
	{
		messaggio->np--;
	}
	else if(messaggio->np<=1)
	{
		messaggio->payload=REPLY_STOP;
		SIGNAL(semid,S_COND_1);
		return 0;
	}
	SIGNAL(semid,S_MUTEX);
	return 0;
}

//Stater process

int stater(int shmid,int semid)
{
	Messaggio*messaggio;
	if((messaggio=(Messaggio*)shmat(shmid,NULL,0))==(Messaggio*)-1)
	{
		fprintf(stderr,"%s: ",program);
		perror("shmat");
		return -1;
	}
	struct stat statBuff;
	while(true)
	{
		WAIT(semid,S_COND_1);
		if(messaggio->payload==REPLY_DATA)
		{
			if((stat(messaggio->path,&statBuff))==-1)
			{
				fprintf(stderr,"%s: ",program);
				perror("stat");
				return -1;
			}
			messaggio->blocco=statBuff.st_blocks;
			messaggio->payload=REPLY_DATA;
		}
		else
		{
			messaggio->payload=REPLY_STOP;
			break;
		}
		SIGNAL(semid,S_COND_2);
	}
	SIGNAL(semid,S_COND_2);
	return 0;
}

//Parent process

void checkDir(bool*isAlive,int nDirs,char**dirs)
{
	isAlive[0]=false;
	for(int i=1; i<=nDirs-1; i++)
	{
		if((access(dirs[i],F_OK))==-1)
		{
			fprintf(stderr,"%s: \"%s\": ",program,dirs[i]);
			perror("");
			isAlive[i]=false;
		}
		else if((access(dirs[i],F_OK))==-1)
		{
			fprintf(stderr,"%s: ",program);
			perror("");
			isAlive[i]=false;
		}
		else
		{
			isAlive[i]=true;
		}
	}
}

int myDuS(int nDirs,char**dirs)
{
	int shmid,semid;
	key_t key=IPC_PRIVATE;
	mode_t mode=S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
	if((shmid=shmget(key,sizeof(Messaggio),IPC_CREAT | IPC_EXCL | mode))==-1)
	{
		fprintf(stderr,"%s: ",program);
		perror("shmget");
		return -1;
	}
	if((semid=semget(key,3,IPC_CREAT | IPC_EXCL | mode))==-1)
	{
		fprintf(stderr,"%s: ",program);
		perror("semget");
		return -1;
	}
	semctl(semid,S_MUTEX,SETVAL,1);
	semctl(semid,S_COND_1,SETVAL,0);
	semctl(semid,S_COND_2,SETVAL,0);
	bool isAlive[nDirs];
	checkDir(isAlive,nDirs,dirs);
	bool anyAlive=false;
	int a=nDirs-1;
	for(int i=1; i<=nDirs-1; i++)
	{
		if(!isAlive[i])
		{
			a--;
		}
	}
	if(a>=2)
	{
		anyAlive=true;
	}
	if(!anyAlive)
	{
		shmctl(shmid,IPC_RMID,NULL);
		semctl(semid,0,IPC_RMID,0);
		return 0;
	}
	Messaggio*messaggio;
	if((messaggio=(Messaggio*)shmat(shmid,NULL,0))==(Messaggio*)-1)
	{
		fprintf(stderr,"%s: ",program);
		perror("shmat");
		return -1;
	}
	messaggio->np=a-1;
	pid_t pid;
	//Stater process
	if((pid=fork())==-1)
	{
		fprintf(stderr,"%s: Can't create a new process: ",program);
		perror("");
		return -1;
	}
	if(!pid)
	{
		if((stater(shmid,semid))==-1)
		{
			shmctl(shmid,IPC_RMID,NULL);
			semctl(semid,0,IPC_RMID,0);
			exit(EXIT_FAILURE);
		}
		exit(EXIT_SUCCESS);
	}
	//Scanner processes
	for(int i=1;i<=nDirs-1;i++)
	{
		if(!isAlive[i])
		{
			continue;
		}
		if((pid=fork())==-1)
		{
			fprintf(stderr,"%s: Can't create a new process: ",program);
			perror("");
			return -1;
		}
		if(!pid)
		{
			if((scanner(shmid,semid,dirs[i],i))==-1)
			{
				shmctl(shmid,IPC_RMID,NULL);
				semctl(semid,0,IPC_RMID,0);
				exit(EXIT_FAILURE);
			}
			exit(EXIT_SUCCESS);
		}
	}
	blkcnt_t blocco[nDirs-1];
	for(int i=0; i<=nDirs-2; i++)
	{
		blocco[i]=0;
	}
	while(true)
	{
		WAIT(semid,S_COND_2);
		if(messaggio->payload==REPLY_DATA)
		{
			blocco[messaggio->n-1]+=messaggio->blocco;
		}
		else
		{
			break;
		}
		SIGNAL(semid,S_MUTEX);
	}
	for(int i=1; i<=nDirs-1; i++)
	{
		if(!isAlive[i])
		{
			continue;
		}
		wait(NULL);
	}
	for(int i=1; i<=nDirs-1; i++)
	{
		if(!isAlive[i])
		{
			continue;
		}
		printf("%ld\t%s\n",blocco[i-1],dirs[i]);
	}
	shmctl(shmid,IPC_RMID,NULL);
	semctl(semid,0,IPC_RMID,0);
	return 0;
}

void help()
{
	printf("Il programma sostanzialmente simula il comportamento del comando du con l'opzione -s: questo per ogni percorso indicato, calcola lo spazio su disco occupato dai file in esso contenuti ricorsivamente.\n");
}

int main(int argc,char**argv)
{
	extern int errno;
	strcpy(program,argv[0]);
	if(argc==1)
	{
		printf("usage: %s [path-1] [path-2] [...]\n",program);
	}
	else if(!strcmp(argv[1],"--help"))
	{
		printf("usage: %s [path-1] [path-2] [...]\n",program);
		help();
	}
	else if((myDuS(argc,argv))==-1)
	{
		exit(EXIT_FAILURE);
	}
	exit(EXIT_SUCCESS);
}