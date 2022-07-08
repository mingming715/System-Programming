/* $begin shellmain */
#include "myshell.h"
#include<errno.h>
#define MAXARGS   128

/* Function prototypes */
void eval(char *cmdline);
int parseline(char *buf, char **argv, char **argv2, int isLastCmd);
int builtin_command(char **argv); 
void execute_pipe(char *argv[][MAXARGS], char ***argv2, int depth, int curr_pos, int in_fd, int bg, char *cmdline);
int isPipeFlag = 0;
int current_child_pid = -1;

typedef struct job {
	pid_t pid;				// Job PID
	int id;					// Job ID
	int state;				// Job STATE
	char cmdline[MAXLINE];	// Job command line
}job;
job job_list[128];

void sigint_handler()
{
	if(current_child_pid!=-1)
		kill(current_child_pid, SIGKILL);

	sio_puts("\n");
	
	return;
}

int main() 
{
	char cmdline[MAXLINE]; /* Command line */
	int original_stdin;
	
	while (1) {
		Signal(SIGINT, sigint_handler);
		printf("CSE4100-SP-P#1> ");
	
		original_stdin = dup(STDIN_FILENO);
		
		/* Read */
		char *pChar = fgets(cmdline, MAXLINE, stdin); 
		
		if (pChar == NULL)
			exit(0);
	
		/* Evaluate */
		isPipeFlag = 0;
		eval(cmdline);
		/* Back to original Stdin */
		Dup2(original_stdin, STDIN_FILENO);
		Close(original_stdin);
	} 
}
/* $end shellmain */

/* $begin eval */
/* eval - Evaluate a command line */
void eval(char *cmdline) 
{
	char *argv[MAXARGS][MAXARGS]; /* Argument list execve() */
	char ***argv2;
	char *pipe_commands[MAXARGS]={};
	char *delim;         /* Points to first '|'  delimiter */
	char buf[MAXLINE];   /* Holds modified command line */
	char *buf_pipe;
	int bg;				/* Should the job run in bg or fg? */
	int depth = 0;
	int child_status;
	pid_t pid;           /* Process id */

	/* argv2 malloc */
	int x, y, z;
	argv2 = (char ***)malloc(sizeof(char **) * MAXARGS);
	for(x=0; x<MAXARGS; x++)
	{
		argv2[x] = (char **)malloc(sizeof(char *)*MAXARGS);
		for(y=0; y<MAXARGS; y++)
		{
			argv2[x][y]=(char *)malloc(sizeof(char)*MAXARGS);
		}
	}

	

	strcpy(buf, cmdline);



	if((delim = strchr(buf, '|'))) {
		isPipeFlag=1;
		buf_pipe = buf;

		int i=0;
		char *p = strtok(buf_pipe, "|");

		while(p != NULL){
			pipe_commands[i++] = p;
			p = strtok(NULL, "|");
			depth++;
		}
		for(i=0; i<depth; i++){  /* No such command with one char */
			if(pipe_commands[i][1] == '\0'){
				printf("No command with one character!\n");
				return;
			}
		}


		int j;
		for(j=0; j<depth-1; j++){	
			bg = parseline(pipe_commands[j], argv[j], argv2[j], 0);
		}


		bg = parseline(pipe_commands[depth-1], argv[depth-1], argv2[depth-1], 1);
		//Last command

		execute_pipe(argv, argv2, depth, 0, 0, bg, cmdline);
	
		return;
	}
	else { /* IF not piped */
		bg = parseline(buf, argv[0], argv2[0], 1);

		if (argv[0][0] == NULL)  
			return;   /* Ignore empty lines */

		if (strcmp(argv2[0][0], "/bin/cd") == 0){ /* cd [dir] or cd .. */
			
			char buf2[MAXLINE];
			char *currDir = getcwd(buf2, sizeof(buf2));
			char *catDir;
			char *finalDir;

			catDir = strcat(currDir, "/");
			finalDir = strcat(catDir, argv[0][1]);
			printf("%s\n", finalDir);
			int result = chdir(finalDir);
			if(result == -1)
				printf("-bash: cd: %s: No such file or directory\n", argv[0][1]);
		}
		else if (!builtin_command(argv2[0])) { //quit -> exit(0), & -> ignore, other -> run

			if((pid=Fork()) == 0){  /* Child runs program */
				if (execve(argv2[0][0], argv[0], environ) < 0) {	//ex) /bin/ls ls -al &

					printf("%s: Command not found.\n", argv2[0][0]);
					exit(0);
				}
			}

			/* Parent waits for foreground job to terminate */
			if (!bg){
				current_child_pid = pid;
				//printf("Parent No. %d\n", getpid());
				int status;
				Waitpid(pid, &child_status, 0);
				current_child_pid=-1;
			}
			else//when there is backgrount process!
				printf("%d %s", pid, cmdline);
		}
	}


	/* Free argv2 */
	for(x=0; x<MAXARGS; x++){
		for(y=0; y<MAXARGS; y++){
			free(argv2[x][y]);
		}
	}
	for(x=0; x<MAXARGS; x++)
		free(argv2[x]);
	
	free(argv2);

	return;
}

void execute_pipe(char *argv[][MAXARGS], char ***argv2, int depth, int curr_pos, int in_fd, int bg, char *cmdline)
{
	int fd1[2];
	pid_t pid;
	
	for(int i=0; i<depth; i++)
	{
		if(strcmp(argv2[i][0], "/bin/cd")==0){
			printf("Cannot pipe cd command...\n");
			return;
		}
		else if(builtin_command(argv2[i])){
			printf("Command error...\n");
			return;
		}
		else
			continue;
	}

	for(int i=0; i<depth; i++)
	{
		if(pipe(fd1) < 0)
			unix_error("pipe error!");

		if((pid = Fork())==0) { //Child
			if(i < depth-1){
				Close(fd1[0]);
				Dup2(fd1[1], STDOUT_FILENO);
				Close(fd1[1]);
			}
			if (execve(argv2[i][0], argv[i], environ) < 0) {	//ex) /bin/ls ls -al &
				printf("%s: Command not found.\n", argv[i][0]);
				exit(0);
			}
		}
		else {//Parent	

			/* Parent waits for foreground job to terminate */
			if (!bg){ 
				int status;
				Close(fd1[1]);
				Dup2(fd1[0], STDIN_FILENO);
				Close(fd1[0]);
				
				Waitpid(pid, &status, 0); 				
			}
			else//when there is backgrount process!
				printf("%d %s", pid, cmdline);

		}
	}
	return;
}
/* If first arg is a builtin command, run it and return true */
int builtin_command(char **argv) 
{
	if (!strcmp(argv[0], "/bin/exit")) /* exit */
		exit(0);
	if (!strcmp(argv[0], "/bin/quit")) /* quit command */
		exit(0);  
	if (!strcmp(argv[0], "/bin/&"))    /* Ignore singleton & */
		return 1;
	return 0;                     /* Not a builtin command */
}
/* $end eval */

/* $begin parseline */
/* parseline - Parse the command line and build the argv array */
int parseline(char *buf, char **argv, char ** argv2, int isLastCmd) 
{
	char *delim;         
	char *doub_quot1, *doub_quot2;	/* Points to double quotation marks */
	char *sing_quot1, *sing_quot2;	/* Points to single quotation marks */
	int argc;						/* Number of args */
	int bg;							/* Background job? */

	if(isLastCmd)
		buf[strlen(buf)-1] = ' ';  /* Replace trailing '\n' with space */

	while (*buf && (*buf == ' ')) /* Ignore leading spaces */
		buf++;
	
	/* Build the argv list */
	argc = 0;
	while ((delim = strchr(buf, ' '))) {
		doub_quot1 = strchr(buf, '\"');
		sing_quot1 = strchr(buf, '\'');
		if((doub_quot1 != NULL) && (doub_quot1 < delim)){ //If there are double quotation marks
			delim = doub_quot1;
			doub_quot2 = strchr(delim+1, '\"');
			*doub_quot2 = '\0';
			argv[argc++] = delim+1;
			buf = doub_quot2+1;
			while (*buf && (*buf == ' ')) /* Ignore spaces */
				buf++;
		}
		else if((sing_quot1 != NULL) && (sing_quot1 < delim)){ //If there are single quotation marks
			delim = sing_quot1;
			sing_quot2 = strchr(delim+1, '\'');
			*sing_quot2 = '\0';
			argv[argc++] = delim+1;
			buf = sing_quot2+1;
			while (*buf && (*buf == ' ')) /* Ignore spaces */
				buf++;
		}
		else {
			argv[argc++] = buf;
			*delim = '\0';
			buf = delim + 1;
			while (*buf && (*buf == ' ')) /* Ignore spaces */
				buf++;
		}
	}

	doub_quot1 = sing_quot1 = NULL; /* Reset */
	if(isLastCmd == 0 && (buf[0] != '\0')){  /* '|' 앞에 공백이 없을 경우, 마지막 단어 추가 */
		doub_quot1 = strchr(buf, '\"'); 
		sing_quot1 = strchr(buf, '\'');
		if(doub_quot1 != NULL){
			doub_quot2 = strchr(doub_quot1+1, '\"');
			*doub_quot2 = '\0';
			argv[argc++] = doub_quot1+1;
		}
		else if(sing_quot1 != NULL){
			sing_quot2 = strchr(sing_quot1+1, '\'');
			*sing_quot2 = '\0';
			argv[argc++] = sing_quot1+1;
		}
		else{
			argv[argc++]=buf;
		}
	}
	argv[argc] = NULL;
	char temp[MAXLINE] = "";

	if((strcmp(argv[0], "sort") == 0) || (strcmp(argv[0], "vi") == 0) || (strcmp(argv[0], "tail") == 0))
	{
		strcpy(temp, "/usr/bin/");
		strcat(temp, argv[0]);
		strcpy(argv2[0], temp);
	}
	else
	{
		strcpy(temp, "/bin/");
		strcat(temp, argv[0]);
		strcpy(argv2[0], temp);
	}
	
	if (argc == 0)  /* Ignore blank line */
		return 1;

	/* Should the job run in the background? */
	if ((bg = (*argv[argc-1] == '&')) != 0) //If it is background
		argv[--argc] = NULL; //Change & into NULL

	return bg;
}
/* $end parseline */


/* $begin forkwrapper */
pid_t Fork(void) 
{
	pid_t pid;

	if ((pid = fork()) < 0)
		unix_error("Fork error");
	return pid;
}
/* $end forkwrapper */


pid_t Waitpid(pid_t pid, int *iptr, int options) 
{
	pid_t retpid;

	if ((retpid  = waitpid(pid, iptr, options)) < 0) 
		unix_error("Waitpid error");
	return(retpid);
}

/* $begin unixerror */
void unix_error(char *msg) /* Unix-style error */
{
	fprintf(stderr, "%s: %s\n", msg, strerror(errno));
	exit(0);
}
/* $end unixerror */



int Dup2(int fd1, int fd2) 
{
	int rc;

	if ((rc = dup2(fd1, fd2)) < 0)
		unix_error("Dup2 error");
	return rc;
}


void Close(int fd) 
{
	int rc;

	if ((rc = close(fd)) < 0)
		unix_error("Close error");
}

/* $begin sigaction */
handler_t *Signal(int signum, handler_t *handler) 
{
    struct sigaction action, old_action;

    action.sa_handler = handler;  
    sigemptyset(&action.sa_mask); /* Block sigs of type being handled */
    action.sa_flags = SA_RESTART; /* Restart syscalls if possible */

    if (sigaction(signum, &action, &old_action) < 0)
	unix_error("Signal error");
    return (old_action.sa_handler);
}
/* $end sigaction */
ssize_t sio_puts(char s[]) /* Put string */
{
    return write(STDOUT_FILENO, s, sio_strlen(s)); //line:csapp:siostrlen
}
/* sio_strlen - Return length of string (from K&R) */
static size_t sio_strlen(char s[])
{
    int i = 0;

    while (s[i] != '\0')
        ++i;
    return i;
}
/* $end sioprivate */
