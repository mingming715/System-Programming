/* $begin shellmain */
#include "myshell.h"
#include<errno.h>
#define MAXARGS   128

/* Function prototypes */
void eval(char *cmdline);
int parseline(char *buf, char **argv);
int builtin_command(char **argv); 
int current_child_pid = -1;

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
	while (1) {
		/* Read */
		printf("CSE4100-SP-P#1> ");                   
		char *pChar = fgets(cmdline, MAXLINE, stdin); 
		if (feof(stdin))
			exit(0);

		/* Evaluate */
		eval(cmdline);
	} 
}
/* $end shellmain */

/* $begin eval */
/* eval - Evaluate a command line */
void eval(char *cmdline) 
{
	char *argv[MAXARGS]; /* Argument list execve() */
	char buf[MAXLINE];   /* Holds modified command line */
	int bg;				/* Should the job run in bg or fg? */
	int child_status;
	pid_t pid;           /* Process id */

	strcpy(buf, cmdline);
	bg = parseline(buf, argv); 
	if (argv[0] == NULL)  
		return;   /* Ignore empty lines */

	if (strcmp(argv[0], "/bin/cd") == 0){ /* cd [dir] or cd .. */
		char buf2[MAXLINE];
		char *currDir = getcwd(buf2, sizeof(buf2));
		char *catDir;
		char *finalDir;

		catDir = strcat(currDir, "/");
		finalDir = strcat(catDir, argv[1]);

		int result = chdir(finalDir);
		if(result == -1)
			printf("-bash: cd: %s: No such file or directory\n", argv[1]);
	}
	else if (!builtin_command(argv)) { //quit -> exit(0), & -> ignore, other -> run

		if((pid=Fork()) == 0){  /* Child runs program */
			//printf("Hello from child\n");
			if (execve(argv[0], argv, environ) < 0) {	//ex) /bin/ls ls -al &
				printf("%s: Command not found.\n", argv[0]);
				exit(0);
			}
		}

		/* Parent waits for foreground job to terminate */
		if (!bg){ 
			int status;
			current_child_pid = pid;
			Signal(SIGINT, sigint_handler);
			
			Waitpid(pid, &child_status, 0); 
			current_child_pid=-1;
		}
		else//when there is backgrount process!
			printf("%d %s", pid, cmdline);
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
int parseline(char *buf, char **argv) 
{
	char *delim;         /* Points to first space delimiter */
	char *doub_quot1, *doub_quot2; /*Points to double quotation marks */
	char *sing_quot1, *sing_quot2; /*Points to single quotation marks */
	int argc;            /* Number of args */
	int bg;              /* Background job? */

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
	argv[argc] = NULL;
	
	char temp[MAXLINE] = "";
	if((strcmp(argv[0], "sort") == 0) || (strcmp(argv[0], "vi") == 0) || (strcmp(argv[0], "tail") == 0)){
		strcpy(temp, "/usr/bin/");
		strcat(temp, argv[0]);
		argv[0] = temp;
	}
	else{
		strcpy(temp, "/bin/");
		strcat(temp, argv[0]);
		argv[0] = temp;
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
/* sio_strlen - Return length of string (from K&R) */
static size_t sio_strlen(char s[])
{
    int i = 0;

    while (s[i] != '\0')
        ++i;
    return i;
}
/* $end sioprivate */

/* Public Sio functions */
/* $begin siopublic */

ssize_t sio_puts(char s[]) /* Put string */
{
    return write(STDOUT_FILENO, s, sio_strlen(s)); //line:csapp:siostrlen
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
