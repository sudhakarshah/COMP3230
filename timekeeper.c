/*
 Filename: timekeeper.c
 Student Name: Sudhakar Shah
 Development Platform: x2go department VM
 Compilation: gcc timekeeper.c -o timekeeper
 Remark: Completed all 3 stages
*/

#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <sys/wait.h>
/*
Code Explaination: We first store all the commands in an array.
Then create child process for each command and set the pipe status accordingly
Then the parent process waits for each child to end by waitid and then prints the stats of each zombie process
*/
int main(int argc, char **argv) {

	// returning if only one argument
	if(argc==1)
		return 0;

	int noOfCommands = 1;
	// counting number of commands
	for(int i=1; i<argc; i++) {
		if(strcmp(argv[i],"!") == 0) {
			argv[i] = NULL;
			noOfCommands++;
		}
	}

	// creating a pointer to a 2d array containing all the commands
	char*** listOfCommands = (char***)malloc(noOfCommands*sizeof(char**));
	int commandNumber = 0;
	int commandStartpos = 1;
	for(int i = 1; i<= argc; i++) {
		if(argv[i] == NULL) {
			listOfCommands[commandNumber] = (char**)malloc((i-commandStartpos)*sizeof(char*));
			for(int j=commandStartpos; j<i; j++) {
				listOfCommands[commandNumber][j-commandStartpos] = (char*)malloc(strlen(argv[j])*sizeof(char));
				strcpy(listOfCommands[commandNumber][j-commandStartpos],argv[j]);
			}
			commandStartpos = i + 1;
			commandNumber++;
		}
	}

	int noOfPipes = noOfCommands-1;
	int** pfdList =  (int**)malloc(noOfPipes*sizeof(int*));
	// creating pipes
	for(int i=0; i<noOfPipes; i++) {
		pfdList[i] = (int*)malloc(2*sizeof(int));
		pipe(pfdList[i]);
	}

	pid_t pid;
	int* childIds = (int*)malloc(noOfCommands*sizeof(int));  // to store the id of the child processes
	struct timespec start, end;
	// creating an array to store start time of all the child processes
	struct timespec* startTime = (struct timespec*)malloc(noOfCommands*sizeof(struct timespec));
	// creating child processes
	for(commandNumber = 0; commandNumber<noOfCommands; commandNumber++) {
		clock_gettime(CLOCK_MONOTONIC, &start);
		startTime[commandNumber] = start;
		pid = fork();

		// parent process
		if(pid > 0) {
			childIds[commandNumber] = pid;
			printf("Process with id: %d created for the command: %s\n", pid, listOfCommands[commandNumber][0]);
		}
		// child process
		else if( pid == 0 ) {
			// if there are pipes
			if (noOfPipes > 0) {
				// if first command
				if(commandNumber == 0) {
					for(int pipeNumber = 0; pipeNumber < noOfPipes; pipeNumber++) {
						if(pipeNumber == 0) {
							dup2(pfdList[pipeNumber][1],1);
							close(pfdList[pipeNumber][0]);
						}
						else {
							close(pfdList[pipeNumber][0]);
							close(pfdList[pipeNumber][1]);
						}
					}
				}
				// Last Command
				else if (commandNumber == noOfCommands - 1) {
					for(int pipeNumber = 0; pipeNumber < noOfPipes; pipeNumber++) {
						if(pipeNumber == noOfPipes - 1) {
							dup2(pfdList[pipeNumber][0],0);
							close(pfdList[pipeNumber][1]);
						}
						else {
							close(pfdList[pipeNumber][0]);
							close(pfdList[pipeNumber][1]);
						}
					}
				}
				else {
					for(int pipeNumber = 0; pipeNumber < noOfPipes; pipeNumber++) {
						if(pipeNumber == commandNumber - 1) {
							dup2(pfdList[pipeNumber][0],0);
							close(pfdList[pipeNumber][1]);
						}
						else if(pipeNumber == commandNumber) {
							dup2(pfdList[pipeNumber][1],1);
							close(pfdList[pipeNumber][0]);
						}
						else {
							close(pfdList[pipeNumber][0]);
							close(pfdList[pipeNumber][1]);
						}
					}
				}
			}
			// executing commands in the child process
			if(execvp(listOfCommands[commandNumber][0],listOfCommands[commandNumber]) == -1) {
				printf("execvp:%s\n", strerror(errno));
				exit(-1);
			}
		}
	}

	// closinf all the pipes in the parent process to make sure the child process will not wait for data from parent
	for(int pipeNumber = 0; pipeNumber < noOfPipes; pipeNumber++) {
					close(pfdList[pipeNumber][0]);
					close(pfdList[pipeNumber][1]);
	}
	// ignoring signals
	signal(SIGINT, SIG_IGN);
	int childNumber = 0;
	for(int commandNumber = 0; commandNumber < noOfCommands; commandNumber++) {
		siginfo_t status;
		// waiting for a child to terminate
		waitid(P_PID, childIds[childNumber], &status, WEXITED | WNOWAIT);
		clock_gettime(CLOCK_MONOTONIC, &end);

		// deciding end status of child
		if(status.si_status == 0) {
			printf("The command %s terminated with returned status code = %d\n",listOfCommands[childNumber][0], status.si_status);
		}
		else if(status.si_status == -1) {
			printf("timekeeper experienced an error in starting the command: %s\n", listOfCommands[childNumber][0]);
		}
		else {
			printf("The command %s is interrupted by the signal number = %d (%s)\n", listOfCommands[childNumber][0], status.si_status, strsignal(status.si_status));
		}

		// getting the path of prog file in the variables
		char statPath[40], statusPath[40];
		sprintf(statPath,"/proc/%d/stat",childIds[childNumber]);
		sprintf(statusPath,"/proc/%d/status",childIds[childNumber]);

		double userTime, systemTime,clockTicksPerSecond;
		clockTicksPerSecond	= sysconf(_SC_CLK_TCK);
		FILE *statFile;
		statFile = fopen(statPath, "r");
		// reading usertime and systemtime
		if(statFile) {
			char x[1024];
			int c = 0;
			while (fscanf(statFile," %1023s",x)==1) {
				c++;
				if(c==14){
					sscanf(x,"%lf",&userTime);
				}
				if(c==15){
					sscanf(x,"%lf",&systemTime);
				}
			}
			userTime = userTime / clockTicksPerSecond;
			systemTime = systemTime / clockTicksPerSecond;
			fclose(statFile);
		}

		int voluntary_ctxt_switches, nonvoluntary_ctxt_switches;
		FILE *statusFile;
		statusFile = fopen(statusPath, "r");
		char temp[1024];
		// getting number of context switches
		while(fgets(temp,1024,statusFile) != NULL){
			fscanf(statusFile, "%s %d %s %d", temp, &voluntary_ctxt_switches,temp, &nonvoluntary_ctxt_switches);
		}
		// subtracting end time - start time
		double realTime = (double)(end.tv_sec - startTime[childNumber].tv_sec) +  (double)(end.tv_nsec - startTime[childNumber].tv_nsec)/1000000000;
		printf("real: %.2fs, user: %.6fs, system: %.6fs, context switch: %d\n", realTime, userTime, systemTime, voluntary_ctxt_switches + nonvoluntary_ctxt_switches);
		int tempStatus;
		// clearing zombie process
		waitpid(childIds[childNumber], &tempStatus, 0);
		childNumber = childNumber + 1;
	}
}
