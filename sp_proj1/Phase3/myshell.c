* $begin shellmain */
#include "myshell.h"
#include<errno.h>
#define MAXARGS   128

/* Function prototypes */
void eval(char *cmdline);
int parseline(char *buf, char **argv, char **argv2, int isLastCmd);
int builtin_command(char **argv, char **argv2); 
void execute_pipe(char *argv[][MAXARGS], char ***argv2, int depth, int curr_pos, int in_fd, int bg, char *cmdline);
void sigint_handler();
void sigchld_handler();
void sigtstp_handler();
/* Global Variables */
int isPipeFlag = 0;
int current_child_pid = -1;
int current_job_index = 0;
typedef struct job {
	pid_t pid;					// Job PID
	int index;					// Job INDEX
	int state;		// Job STATE(0:Default, 1:FG, 2:BG, 3:Done, 4:Suspended, 5:Killed)
	char cmdline[MAXLINE];		// Job command line
}job;
job job_list[128];
void job_killed_done(job *job_list);
void job_print_all(job *job_list);
void job_clear(job *job);
int job_add(job *job_list, pid_t pid, int state, char *cmdline);

int main() 
{
	char cmdline[MAXLINE]; /* Command line */
	int original_stdin;

	for(int i=0; i<128; i++)	/* Initialize */
		job_clear(&job_list[i]);

	while (1) {
		Signal(SIGINT, sigint_handler);
		Signal(SIGTSTP, sigtstp_handler);
		Signal(SIGCHLD, sigchld_handler);
		printf("CSE4100-SP-P#1> ");

		original_stdin = dup(STDIN_FILENO);

		/* Read */
		char *pChar = fgets(cmdline, MAXLINE, stdin); 

		if (pChar == NULL)
			exit(0);

		/* Evaluate */
		isPipeFlag = 0;
		eval(cmdline);
		job_killed_done(job_list);
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

		int state;

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

		if(bg != 0) state=2; //if BG
		else state=1; //if FG
		if((pid=Fork()) == 0){
			if(!bg) Signal(SIGINT, SIG_DFL);
			else Signal(SIGINT, SIG_IGN); /* Cannot terminate BG with Ctrl+C */
			execute_pipe(argv, argv2, depth, 0, 0, bg, cmdline);
			exit(0);	
		}
		else{
			job_add(job_list, pid, state, cmdline);
			if(!bg) {
				if(waitpid(pid, &child_status, WUNTRACED)<0){
					unix_error("waitpid error");
				}
				for(int i=0; i<128; i++){
					if(job_list[i].state == 1)
						job_clear(&job_list[i]);
				}
			}
			else//when there is backgrount process!
			{
				int index;
				int i;
				for(i=0; i<128; i++){
					if(job_list[i].pid == pid){
						index=job_list[i].index;
						break;
					}
				}

				if(job_list[i].state == 2)
					printf("[%d] Running\t %s", index, cmdline);
				else if(job_list[i].state == 4)
					printf("[%d] Stopped\t %s", index, cmdline);
			}
		}
		return;
	}
	else { /* IF not piped */
		bg = parseline(buf, argv[0], argv2[0], 1);
		int state;
		if(bg != 0) state=2; //if BG
		else state=1; //if FG

		if (argv[0][0] == NULL)  
			return;   /* Ignore empty lines */

		if (strcmp(argv2[0][0], "/bin/cd") == 0){ /* cd [dir] or cd .. */
			char buf2[MAXLINE];
			char *currDir = getcwd(buf2, sizeof(buf2));
			char *catDir;
			char *finalDir;

			catDir = strcat(currDir, "/");
			finalDir = strcat(catDir, argv[0][1]);

			int result = chdir(finalDir);
			if(result == -1)
				printf("-bash: cd: %s: No such file or directory\n", argv[0][1]);
		}
		else if (!builtin_command(argv[0], argv2[0])) { //quit -> exit(0), & -> ignore, other -> run

			if((pid=Fork()) == 0){  /* Child runs program */
				if(!bg) Signal(SIGINT, SIG_DFL);
				else Signal(SIGINT, SIG_IGN); /* Cannot terminate BG with Ctrl+C */

				if (execve(argv2[0][0], argv[0], environ) < 0) {	//ex) /bin/ls ls -al &

					printf("%s: Command not found.\n", argv2[0][0]);
					exit(0);
				}
			}

			job_add(job_list, pid, state, cmdline);

			/* Parent waits for foreground job to terminate */
			if (!bg){
				current_child_pid = pid;
				if(waitpid(pid, &child_status, WUNTRACED)<0){
					unix_error("waitpid error");
				}
				if(WIFEXITED(child_status)!=0){		/* If FG job reaped, remove that job from list */	
					for(int i=0; i<128; i++){
						if(job_list[i].state = 1){
							job_clear(&job_list[i]);
						}
					}
				}
				current_child_pid=-1;
			}
			else {//when there is backgrount process!
				int index;
				int i;
				for(i=0; i<128; i++){
					if(job_list[i].pid == pid){
						index=job_list[i].index;
						break;
					}
				}
				if(job_list[i].state == 2)
					printf("[%d] Running\t %s", index, cmdline);
				else if(job_list[i].state == 4)
					printf("[%d] Stopped\t %s", index, cmdline);
			}
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
	int status;
	pid_t pid;

	for(int i=0; i<depth; i++)
	{
		if(strcmp(argv2[i][0], "/bin/cd")==0){
			printf("Cannot pipe cd command...\n");
			return;
		}
		else if(builtin_command(argv[i], argv2[i])){
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
			current_child_pid = pid;

			/* Parent waits for foreground job to terminate */
			Close(fd1[1]);
			Dup2(fd1[0], STDIN_FILENO);
			Close(fd1[0]);

			if(waitpid(pid, &status, 0)<0)
				unix_error("waitpid error"); 
		}
	}
	current_child_pid=-1;
	return;
}
/* If first arg is a builtin command, run it and return true */
int builtin_command(char **argv, char **argv2) 
{
	if (!strcmp(argv2[0], "/bin/exit")) /* exit */
		exit(0);
	if (!strcmp(argv2[0], "/bin/quit")) /* quit command */
		exit(0);  
	if (!strcmp(argv2[0], "/bin/&"))    /* Ignore singleton & */
		return 1;
	if (!strcmp(argv2[0], "/bin/jobs")){
		job_print_all(job_list);
		return 1;
	}
	if (!strcmp(argv2[0], "/bin/kill")){
		int kill_index;
		kill_index = atoi(&argv[1][1]);
		if(job_list[kill_index-1].state == 0) //If not such job
			printf("-bash: kill: %s: no such job\n", argv[1]);
		else {
			kill(job_list[kill_index-1].pid, SIGKILL);
			job_list[kill_index-1].state = 5;
		}
		return 1;
	}
	if (!strcmp(argv2[0], "/bin/fg")){
		int fg_index;
		fg_index = atoi(&argv[1][1]);
		if(job_list[fg_index-1].state == 0) //If not such job
			printf("-bash: fg: %s: no such job\n", argv[1]);
		else {
			char *ptr;
			if(ptr=strchr(job_list[fg_index-1].cmdline, '&'))
				*ptr = '\0';
			printf("%s\n", job_list[fg_index-1].cmdline);
			job_list[fg_index-1].state = 1; //Change state to FG
			kill(job_list[fg_index-1].pid, SIGCONT);

			/* Wait for the new FG job to be done */
			while(job_list[fg_index-1].state == 1){
				usleep(100000); //Check every 0.1 sec (==100000microsec)
			}
		}
		return 1;
	}
	if (!strcmp(argv2[0], "/bin/bg")){
		int bg_index;
		bg_index = atoi(&argv[1][1]);
		if(job_list[bg_index-1].state == 2) //Already in BG
			printf("-bash: bg: job %d already in background\n", job_list[bg_index-1].index);
		else if(job_list[bg_index-1].state == 0) //If not such job
			printf("-bash: bg: %s: no such job\n", argv[1]);
		else {
			job_list[bg_index-1].cmdline[strlen(job_list[bg_index-1].cmdline)-1] = ' ';
			job_list[bg_index-1].cmdline[strlen(job_list[bg_index-1].cmdline)] = '&';
			job_list[bg_index-1].cmdline[strlen(job_list[bg_index-1].cmdline)+1] = '\0';
			printf("%s\n", job_list[bg_index-1].cmdline);
			job_list[bg_index-1].state = 2; //Change state to BG
			kill(job_list[bg_index-1].pid, SIGCONT);
		}
		return 1;
	}

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

	char last_char = argv[argc-1][strlen(argv[argc-1])-1];

	if(last_char == '&')
		argv[argc-1][strlen(argv[argc-1])-1] = '\0';

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
	if ((bg = (last_char == '&')) != 0){ //If it is background
		//printf("this is background");
		if(*argv[argc-1] == '\0')
			argv[--argc] = NULL; //Change & into NULL
	}
	return bg;
}
/* $end parseline */

void job_print_all(job *job_list)
{
	for(int i=0; i<128; i++){
		if(job_list[i].state == 1) //If FG
			printf("[%d] Foreground\t\t%s\n", job_list[i].index, job_list[i].cmdline);
		else if(job_list[i].state == 2) //If BG
			printf("[%d] Running\t\t%s\n", job_list[i].index, job_list[i].cmdline);
		else if(job_list[i].state == 4) //If Suspended
			printf("[%d] Suspended\t\t%s\n", job_list[i].index, job_list[i].cmdline);
	}
}

void job_killed_done(job *job_list)
{
	char *ptr;
	for(int i=0; i<128; i++){
		if(job_list[i].state == 3){	//If DONE
			job_clear(&job_list[i]);
		}
		else if(job_list[i].state == 5){	//If KILLED
			job_clear(&job_list[i]);
		}
	}
	return;
}

void job_clear(job *job)
{
	job->pid =0;
	job->index=0;
	job->state=0;
	job->cmdline[0]='\0';
}

int job_add(job *job_list, pid_t pid, int state, char *cmdline)
{
	if(pid<1) return 0;
	if(job_list[current_job_index].state == 0) {
		job_list[current_job_index].index = current_job_index+1;
		job_list[current_job_index].pid = pid;
		job_list[current_job_index].state = state;
		strcpy(job_list[current_job_index].cmdline, cmdline);
		current_job_index++;
		return 1;
	}
	return 0;
}

void sigint_handler()
{
	for(int i=0; i<128; i++){
		if(job_list[i].state == 1){
			kill(job_list[i].pid,SIGKILL);
			job_clear(&job_list[i]);
		}
	}
	sio_puts("\n");
	return;
}


void sigchld_handler()
{
	pid_t pid;
	int status, index;
	while((pid = waitpid(-1, &status, WNOHANG | WCONTINUED)) > 0){
		if(WIFEXITED(status)){ /* Child process terminated normally */
			for(int i=0; i<128; i++){
				if(job_list[i].pid == pid){
					job_list[i].state = 3; /* Change state to DONE */
					break;
				}
			}
		}
		else if(WIFSIGNALED(status)){ /* Child process terminated abnormally */
			for(int i=0; i<128; i++){
				if(job_list[i].pid == pid){
					job_list[i].state = 3; /* Change state to DONE */
					break;
				}
			}
		}
	}
	return;
}

void sigtstp_handler()
{
	sio_puts("\n");
	for(int i=0; i<128; i++){
		if(job_list[i].state == 1){
			job_list[i].state=4; /* Change state to SUSPENDED */
			kill(job_list[i].pid,SIGTSTP);
			printf("[%d] Suspended\t\t%s\n", job_list[i].index, job_list[i].cmdline);
		}
	}

	return;

}
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
