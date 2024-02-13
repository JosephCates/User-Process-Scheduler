/*Name : Joseph Cates, 
  Duck Id:jcates5,
  Project Name: CIS415 Project 1
  All my own work, except the alarm handler which was taken from Timmys' lab4
  and the sigchild hanlder which was taken from the just chapter 8 reading*/


#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<fcntl.h>
#include<sys/wait.h>
#include<sys/time.h>
#include<sys/types.h>
#include "ADTs/queue.h"
#include"p1fxns.h"
#include<errno.h>


#define UNUSED __attribute__ ((unused))
#define MIN_QUANTUM 10
#define MAX_QUANTUM 1000
#define MS_PER_TICK 20


volatile int activeProcess = 0;
int commandCount = 0;
const Queue *q = NULL;
int ticksInQuantum = 0;


typedef struct pcb{
	char**commandBroken;
	int wc;
	pid_t pid;
	int time;
	int timesRan;
	int killed;
} PCB;

PCB *commandCurrent = NULL;


static void onAlarm(UNUSED int sig){
	if(commandCurrent != NULL){
		if(!commandCurrent ->killed){
			--commandCurrent ->time;
			if(commandCurrent ->time > 0){
				return;
			}
			kill(commandCurrent->pid,SIGSTOP);
			q->enqueue(q, ADT_VALUE(commandCurrent));
		}
		commandCurrent= NULL;
	}
	while(q -> dequeue(q, ADT_ADDRESS(&commandCurrent))){
		if(commandCurrent ->killed == 1){
			continue;
		}
		commandCurrent->time = ticksInQuantum;
		if(commandCurrent->timesRan == 0){
			commandCurrent->timesRan = 1;
			kill(commandCurrent ->pid,SIGUSR1);
		}else{
			kill(commandCurrent ->pid,SIGCONT);
		}
	return;
	}
}

static void childHandler(UNUSED int sig){
	pid_t pid;
	int status;

	while((pid = waitpid(-1, &status, WNOHANG)) > 0) {
		if(WIFEXITED(status)){
			commandCurrent -> killed = 1;
			activeProcess--;
		}
	}
}



int executePrograms(PCB commandList[BUFSIZ],int commandCount){
	int sig;
	struct itimerval itVal;
	sigset_t newSet;
	sigemptyset(&newSet);
	sigaddset(&newSet, SIGUSR1);
	sigprocmask(SIG_BLOCK, &newSet, NULL);
	if(signal(SIGUSR1,NULL) == SIG_ERR){
		errno = ENOMSG;
		p1perror(2,"Unable to create user1 singal handler");
		return 0;
	}
	if(signal(SIGALRM,onAlarm) == SIG_ERR){
		errno = ENOMSG;
		p1perror(2,"Unable to create alarm singal handler");
		return 0;
	}
	if((signal(SIGCHLD, childHandler)) == SIG_ERR){
		errno = ENOMSG;
		p1perror(2,"Unable to create child singal handler");
		return 0;
	}
	for(int i = 0; i < commandCount; i++){
		commandList[i].pid = fork();
		if(commandList[i].pid == 0){
			sigwait(&newSet, &sig);
			execvp(commandList[i].commandBroken[0],commandList[i].commandBroken);
		}else if(commandList[i].pid == -1){
			errno = ENOMSG;
			p1perror(2, "failed to fork");
		}
	}
	itVal.it_value.tv_sec = MS_PER_TICK/1000;
	itVal.it_value.tv_usec = (MS_PER_TICK * 1000) % 10000000;
	itVal.it_interval = itVal.it_value;
	if((setitimer(ITIMER_REAL, &itVal, NULL)) == -1){
		errno = ENOMSG;
		p1perror(2,"Unable to create timer");
		for(int i = 0; i < commandCount; i++){
			kill(commandList[i].pid, SIGKILL);
		}	
	}
	
	onAlarm(SIGALRM);

	while(activeProcess){
		pause();
	}
	return 0;
}
void readFile(int fd, int time){
	int newtime = MS_PER_TICK * ((time + 1)/ MS_PER_TICK);
	ticksInQuantum = newtime / MS_PER_TICK;
	char command[BUFSIZ];
	char word[BUFSIZ];
	PCB commandList[BUFSIZ];
	int byteNum;
	int wordCount = 0;
	int index = 0;
	q = Queue_create(doNothing);
	while((byteNum = p1getline(fd,command,512)) != 0){
		commandList[commandCount].commandBroken = malloc(512);
		if(command[byteNum-1] == '\n'){
			command[byteNum-1] = '\0';
		} 
		while((index = p1getword(command,index,word)) != -1){
			commandList[commandCount].commandBroken[wordCount] = malloc(512);
			p1strcpy(commandList[commandCount].commandBroken[wordCount],word);
			wordCount++;
		}
		commandList[commandCount].commandBroken[wordCount] = NULL;
		commandList[commandCount].wc = wordCount;
		commandList[commandCount].killed = 0;
		commandList[commandCount].timesRan = 0;
		q -> enqueue(q, ADT_VALUE(&commandList[commandCount]));
		commandCount++;
		activeProcess++;
		index = 0;
		wordCount = 0;
	}
	executePrograms(commandList,commandCount);
	for(int i = 0; i < commandCount; i++){
		for(int x = 0; x < commandList[i].wc; x++){
			free(commandList[i].commandBroken[x]);
		}
		free(commandList[i].commandBroken);
	}
	q -> destroy(q);
}

int main(int argc, char **argv){
	int quantumEnvVal = -1;
	char *quantumValStr;
	if((quantumValStr = getenv("USPS_QUANTUM_MSEC"))!= NULL){
		quantumEnvVal = p1atoi(quantumValStr);
	}
	int quantumFlagVal = -1;
	int opt = getopt(argc,argv,"q");
	if(opt != -1){
		quantumFlagVal = p1atoi(argv[2]);
	}
	if(quantumEnvVal == -1 && quantumFlagVal == -1){
		errno = EINVAL;
		p1perror(2,"No quantum was given");
		return 0;
	}
	if(quantumEnvVal != -1){
		if(quantumEnvVal < MIN_QUANTUM || quantumEnvVal > MAX_QUANTUM) {
			errno = EDOM;
			p1perror(2, "Quantum should be between 10 and 1000");
			return 0;
		}
	}else{
		if(quantumFlagVal < MIN_QUANTUM || quantumFlagVal > MAX_QUANTUM){
			errno = EDOM;
			p1perror(2, "Quantum should be between 10 and 1000");
			return 0;
		}
	}
	int file;
	if(quantumFlagVal == -1 && argc == 2){
		//have a filename, but no quantum flag
		file = open(argv[1], O_RDONLY);
		if(file != -1){
			readFile(file, quantumEnvVal);
			close(file);
		}else{
			errno = ENOENT;
			p1perror(2, "Unable to open file");
			return 0;
		}
	}else if(quantumFlagVal != -1 && argc == 4){
		//have a filename and quantum flag
		file = open(argv[3],O_RDONLY);
		if(file != -1){
			readFile(file, quantumFlagVal);
			close(file);
		}else{
			errno = ENOENT;
			p1perror(2, "Unable to open file");
			return 0;
		}

	}else{
		if(quantumEnvVal != -1){
			readFile(0, quantumEnvVal);
		}else{
			readFile(0, quantumFlagVal);
		}
	}
}



