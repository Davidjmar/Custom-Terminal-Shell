/* 
 * tsh - A tiny shell program with job control
 * 
 * <David Martin dama7453>
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

/* Misc manifest constants */
#define MAXLINE    1024   /* max line size */
#define MAXARGS     128   /* max args on a command line */
#define MAXJOBS      16   /* max jobs at any point in time */
#define MAXJID    1<<16   /* max job ID */

/* Job states */
#define UNDEF 0 /* undefined */
#define FG 1    /* running in foreground */
#define BG 2    /* running in background */
#define ST 3    /* stopped */

/* Builtin types */
#define BLTN_UNK 0
#define BLTN_IGNR 1
#define BLTN_BGFG 2
#define BLTN_JOBS 3
#define BLTN_EXIT 4
#define BLTN_KILLALL 5

/* 
 * Jobs states: FG (foreground), BG (background), ST (stopped)
 * Job state transitions and enabling actions:
 *     FG -> ST  : ctrl-z
 *     ST -> FG  : fg command
 *     ST -> BG  : bg command
 *     BG -> FG  : fg command
 * At most 1 job can be in the FG state.
 */

/* Global variables */
extern char **environ;      /* defined in libc */
char prompt[] = "tsh> ";    /* command line prompt (DO NOT CHANGE) */
int verbose = 0;            /* if true, print additional output */
int nextjid = 1;            /* next job ID to allocate */
char sbuf[MAXLINE];         /* for composing sprintf messages */

struct job_t {              /* The job struct */
    pid_t pid;              /* job PID */
    int jid;                /* job ID [1, 2, ...] */
    int state;              /* UNDEF, BG, FG, or ST */
    char cmdline[MAXLINE];  /* command line */
};
struct job_t jobs[MAXJOBS]; /* The job list */
/* End global variables */


/* Function prototypes */

/* Here are the functions that you will implement */
void eval(char *cmdline);
int is_builtin_cmd(char **argv);
void do_exit(void);
void do_show_jobs(void);
void do_ignore_singleton(void);
void do_killall(char **argv);
void do_bgfg(char **argv);
void waitfg(pid_t pid);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);
void sigalrm_handler(int sig);

/* Here are helper routines that we've provided for you */
int parseline(const char *cmdline, char **argv); 
void sigquit_handler(int sig);

void clearjob(struct job_t *job);
void initjobs(struct job_t *jobs);
int maxjid(struct job_t *jobs); 
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline);
int removejob(struct job_t *jobs, pid_t pid); 
pid_t fgpid(struct job_t *jobs);
struct job_t *getprocessid(struct job_t *jobs, pid_t pid);
struct job_t *getjobid(struct job_t *jobs, int jid); 
int get_jid_from_pid(pid_t pid); 
void showjobs(struct job_t *jobs);

void usage(void);
void unix_error(char *msg);
void app_error(char *msg);
typedef void handler_t(int);
handler_t *Signal(int signum, handler_t *handler);

/*
 * main - The shell's main routine 
 */
int main(int argc, char **argv) 
{
    char c;
    char cmdline[MAXLINE];
    int emit_prompt = 1; /* emit prompt (default) */

    /* Redirect stderr to stdout (so that driver will get all output
     * on the pipe connected to stdout) */
    dup2(1, 2);

    /* Parse the command line */
    while ((c = getopt(argc, argv, "hvp")) != EOF) {
        switch (c) {
        case 'h':             /* print help message */
            usage();
	    break;
        case 'v':             /* emit additional diagnostic info */
            verbose = 1;
	    break;
        case 'p':             /* don't print a prompt */
            emit_prompt = 0;  /* handy for automatic testing */
	    break;
	default:
            usage();
	}
    }

    /* Install the signal handlers */

    /* These are the ones you will need to implement */
    Signal(SIGINT,  sigint_handler);   /* ctrl-c */
    Signal(SIGTSTP, sigtstp_handler);  /* ctrl-z */
    Signal(SIGCHLD, sigchld_handler);  /* Terminated or stopped child */
    Signal(SIGALRM, sigalrm_handler);  /* Alarm indicates killing all children */

    /* This one provides a clean way to kill the shell */
    Signal(SIGQUIT, sigquit_handler); 

    /* Initialize the job list */
    initjobs(jobs);

    /* Execute the shell's read/eval loop */
    while (1) {

	/* Read command line */
	if (emit_prompt) {
	    printf("%s", prompt);
	    fflush(stdout);
	}
	if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
	    app_error("fgets error");
	if (feof(stdin)) { /* End of file (ctrl-d) */
	    fflush(stdout);
	    exit(0);
	}

	/* Evaluate the command line */
	eval(cmdline);
	fflush(stdout);
	fflush(stdout);
    } 

    exit(0); /* control never reaches here */
}
  
/* 
 * eval - Evaluate the command line that the user has just typed in
 * 
 * If the user has requested a built-in command (exit, jobs, bg or fg)
 * then execute it immediately. Otherwise, fork a child process and
 * run the job in the context of the child. If the job is running in
 * the foreground, wait for it to terminate and then return.  Note:
 * each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.  
*/
void eval(char *cmdline) 
{
	//first things first let's parse the command line into its component parts
	//this will break it down into the path to the command we want to execute 
	//and the rest is command line arguments that give the command the info it needs
	//in order to execute.
	//paresLine does this already but we need to declare the argv array that gets
	//passed to parseline
	//the array is made up of pointers to characters
	//these pointers will be populated by parseline
	//so let's create the array
    char bfer[MAXLINE];
    char *argv[MAXARGS];
    strcpy(bfer, cmdline);
    int backg = parseline(cmdline, argv);
    //call the parseLine function to then break uppp the input and figure out what needs to be done
    pid_t pidVal;
    //The job_t struct was created in order to utilize the job pointer as a bit of a "dynamic incrementer"
	//the job pointer is primarily useful in eval in the final else statement to print out the jid and the pid's of the process.
	//This will essentially just make it easier and I beleive a bit more efficent in the accessing information process
    struct job_t *job;
    //in case of NULL input
    if(argv[0] == NULL){
        return;
    }
    //now that we know it is not a built in command 
		//we should handle the forking and execing a child process
		//forking the child process and this if below
		//tells us if we are in the child process
    if(!is_builtin_cmd(argv)){
        if((pidVal = fork()) == 0){
            setpgid(0,0);
            if(execvp(argv[0],argv) <0){
                printf("%s: Command not found\n", argv[0]);
                //if we don't try to check if the command is legal
				//and the exec fails, it will simply go past the code and it will begin reading the command
				//and forking and execing like a recursive shell
				//this means each time you type in a unknown command the tsh will run
				//and multiple tsh's will be created with no way to quit
				//this code is to break it out of that loop if the command is not found
            }
            do_exit();
        }
        //here we will write a function for the parent to 
		//wait for the child process and reap it at the same time which 
		//we can do by using the waitfg(pid) function call
        if(backg ==0){
            addjob(jobs,pidVal,FG,cmdline);
            waitfg(pidVal);
            //this foreground specific wait function will allow the child to fully run and be reaped 
			//otherwise this is how we get multiple tsh's running shown in the simple /bin/ps command
        }
        else{
            addjob(jobs,pidVal,BG,cmdline);
            int jid = get_jid_from_pid(pidVal);
            job = getjobid(jobs,jid);
            printf("[%d] (%d) %s", job->jid, job->pid, cmdline);
        }
    }
    return;
}

/* 
 * parseline - Parse the command line and build the argv array.
 * 
 * Characters enclosed in single quotes are treated as a single
 * argument.  Return true if the user has requested a BG job, false if
 * the user has requested a FG job.  
 */
int parseline(const char *cmdline, char **argv) 
{
    static char array[MAXLINE]; /* holds local copy of command line */
    char *buf = array;          /* ptr that traverses command line */
    char *delim;                /* points to first space delimiter */
    int argc;                   /* number of args */
    int bg;                     /* background job? */

    strcpy(buf, cmdline);
    buf[strlen(buf)-1] = ' ';  /* replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* ignore leading spaces */
	buf++;

    /* Build the argv list */
    argc = 0;
    if (*buf == '\'') {
	buf++;
	delim = strchr(buf, '\'');
    }
    else {
	delim = strchr(buf, ' ');
    }

    while (delim) {
	argv[argc++] = buf;
	*delim = '\0';
	buf = delim + 1;
	while (*buf && (*buf == ' ')) /* ignore spaces */
	       buf++;

	if (*buf == '\'') {
	    buf++;
	    delim = strchr(buf, '\'');
	}
	else {
	    delim = strchr(buf, ' ');
	}
    }
    argv[argc] = NULL;
    
    if (argc == 0)  /* ignore blank line */
	return 1;

    /* should the job run in the background? */
    if ((bg = (*argv[argc-1] == '&')) != 0) {
	argv[--argc] = NULL;
    }
    return bg;
}


/*
 * is_builtin_cmd - If the user has typed a built-in command then
 * return the type of built in command, otherwise indicate that it
 * isn't a built in command
 */
int is_builtin_cmd(char **argv)
{
	//originally I was concerned that this builtin cmnd function was order sensitive during the test03 process
	//however it is not at all and they may be in any order desired
	 //exit
    if(strcmp("exit", argv[0]) == 0)
    {
        do_exit();
        return 0;
    }
    //kill all
    if(strcmp("killall", argv[0]) == 0)
    {
        do_killall(argv);
        return 1;
    }
    //jobs
    if(strcmp("jobs", argv[0]) == 0)
    {
        do_show_jobs();
        return 1;
    }
    //background
    if(strcmp("bg", argv[0]) == 0)
    {
        do_bgfg(argv);
        return 1;
    }
    //foreground
    if(strcmp("fg", argv[0]) == 0)
    {
        do_bgfg(argv);
        return 1;
    }
    return BLTN_UNK;     /* not a builtin command */
}

/*
 * do_exit - Execute the builtin exit command
 */
void do_exit(void)
{
// this initial case is vital to set first as it allows you to quit later on
	//when doing development testing
  exit(0);
}

/*
 * do_show_jobs - Execute the builtin jobs command
 */
void do_show_jobs(void)
{
	//this simply "cheats" off the pre-existing showjobs function
	//written for this lab
	//I originally had intended to utilize a for Loop for this until I read instructions
	//as well as actually really looked at this code
    showjobs(jobs);
}

/*
 * do_ignore_singleton - Display the message to ignore a singleton '&'
 */
void do_ignore_singleton(void)
{   
	//this is important to making sure the shell doesn't freak out with singleton &'s
    if(prompt[0] ==  '&'){
    	//app_error was again another provided function vital in making my life easier
        app_error("ignoring singleton '&'!");
    }
  return;
}

void do_killall(char **argv)
{
	//kill all utilizes a timeout killall approach
	//i.e. the resoning for the timeout alarm value
    int timeoutAlm = atoi(argv[1]);
    alarm(timeoutAlm);
    return;
}

/* 
 * do_bgfg - Execute the builtin bg and fg commands
 */
void do_bgfg(char **argv) 
{
    if((argv[1]) == NULL){
        //Edge case
        printf("%s command requires PID or Jjobid argument\n", argv[0]);
        return;
    }
    int piDTrue = 1;
     //initialize the integer which will behave as a switch for JID or PID
    //the if statement below will determine if the input is a PID
    //if it is, it will set piDInput to 1
    if(isdigit(argv[1][0])!=0){
        piDTrue=1;
    }
    else if((argv[1][0]) == 'J'){
        piDTrue = 0;      
    }
    else{
        printf("%s argument must be PID or Jjobid\n", argv[0]);
        return;
    }
    pid_t pidVal;
    struct job_t *job;
    int jiD;
    //all of these intitializations are vital to the final if statement of processing either a JID or PID to do either bg or fg
    if(piDTrue){
        pidVal = atoi(argv[1]);
        job = getprocessid(jobs, pidVal);
        if(job == NULL){
            printf("(%s): No such process\n", argv[1]);
            return;
        }
    }
    //this else pertains to whether the argument was a PID or JID argument from the user
    //this means in this else statement it is processing a JID argument
    else{
        jiD = atoi(&argv[1][1]);
        job = getjobid(jobs,jiD);
        if(job == NULL){
            printf("%s: No such job\n", argv[1]);
            return;
        }
        pidVal = job->pid;
    }
    //using a simple strcmp we can determine if the user inputted the bg or fg command
    //in this if statement, if it is entered, it follows that it will utilize sigcont and kill the process and report the jid, pid, cmdline and then set the job state
    if(strcmp(argv[0], "bg") ==0){
        kill(-pidVal, SIGCONT);
        printf("[%d] (%d) %s", job->jid, job->pid,job->cmdline);
        job->state = BG;
    }
    else{
    	//it will do the same prtocess as the if statement before except
    	//it will perform a waitFG as the command was indicated that it should be run in the foreground.
        kill(-pidVal, SIGCONT);
        job->state =FG;
        waitfg(pidVal);
    }
    return;
}

/* 
 * waitfg - Block until process pid is no longer the foreground process
 */
void waitfg(pid_t pid)
{
	//this will create a foreground sleep where the shell will wait until the foreground process is completed
    struct job_t *job = getprocessid(jobs, pid);
    while(job->state == FG){
        sleep(1);
    }
    return;
}

/*****************
 * Signal handlers
 *****************/

/* 
 * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
 *     a child job terminates (becomes a zombie), or stops because it
 *     received a SIGSTOP or SIGTSTP signal. The handler reaps all
 *     available zombie children, but doesn't wait for any other
 *     currently running children to terminate.  
 */
void sigchld_handler(int sig) 
{
    pid_t pidVal;
    int stVal;
    //because there are multiple children possible
    //we need to utilize a while statement in order to properly 
    //stop, reap zombie children, or kill due to a SIGINT 
    while ((pidVal = waitpid(-1, &stVal, WNOHANG|WUNTRACED)) > 0)
    {
        if(WIFSIGNALED(stVal))
        {
        	//SIGINT signal value == 2
            int jidVal = get_jid_from_pid(pidVal);
            printf("Job [%d] (%d) terminated by signal 2\n", jidVal, pidVal);
            removejob(jobs, pidVal);
        }
        else if(WIFSTOPPED(stVal))
        {
        	//SIGSTP VAL == 20
            int jidVal = get_jid_from_pid(pidVal);
            printf("Job [%d] (%d) stopped by signal 20\n", jidVal, pidVal);
            sigtstp_handler(20);
        }
        else if(WIFEXITED(stVal))
        {
            removejob(jobs, pidVal);
        }
    }
    return;
}

/*
 * sigalrm_handler - The kernel sends a SIGALRM to the shell after
 * alarm(timeout) times out. Catch it and send a SIGINT to every
 * EXISTING (pid != 0) job
 */

void sigalrm_handler(int sig)
{
    struct job_t *job;
    pid_t pidVal;
    //if there are no zombie children or currently running jobs no need to kill all
    if(maxjid(jobs) == 0)
    {
        return;
    }
    //utilize the for loop to iterate through all of the available jobs and kill them
    for(int i =1; i < MAXJOBS; i++)
    {
    	//if the job id is equivalent to NULL there is no need to run a kill 
    	//so simply return before trying to kill a NULL
    	//this tripped me up for a long while.
        if((job =getjobid(jobs, i)) == NULL)
        {
            return;
        }
        // utilize the job pointer to access the PID value and store in the 
        //pidVal integer to be passed to the kill function
        pidVal = job->pid;
        kill(pidVal, SIGINT);
    }
    return;
}

/* 
 * sigint_handler - The kernel sends a SIGINT to the shell whenver the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.  
 */
void sigint_handler(int sig) 
{
	//this is a relatively simplistic approach to handling a SIGINT call
	//the first thing we need to do is acquire the foregroud pid and store it in 
	//pidVal to be passed to kill with the SIGINT value after making sure the pid is 
	//non-zero otherwise there would be no need for the kill to occur
	//thus why the return lives outside of the for loop
    pid_t pidVal = fgpid(jobs);
    if(pidVal != 0)
    {    
        kill(-pidVal, SIGINT);
    }
    return;
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.  
 */
void sigtstp_handler(int sig) 
{
	//again we use the pid_t struct to set the pid
	//and set it using the foreground pid function
	//we can then acquire the jid using this pidVal
    pid_t pidVal = fgpid(jobs);
    int jidVal = get_jid_from_pid(pidVal);
    struct job_t *job;
    //again we just make sure that the pidVal is not already 0'd before running
    //kill processes
    if(pidVal != 0){
        job = getjobid(jobs, jidVal);
        kill(-pidVal, SIGTSTP);
        job->state = ST;
        //and then I need to make sure that I indicate the job state is now set to stopped
        //in case later use is intended
    }
    //an added return after the if statement is useful to ensuring the cases where
    //there are no foreground jobs to suspend so it can instead simply return 
    //rather than attempting to kill non-existing processes
    return;
}

/*********************
 * End signal handlers
 *********************/

/***********************************************
 * Helper routines that manipulate the job list
 **********************************************/

/* clearjob - Clear the entries in a job struct */
void clearjob(struct job_t *job) {
    job->pid = 0;
    job->jid = 0;
    job->state = UNDEF;
    job->cmdline[0] = '\0';
}

/* initjobs - Initialize the job list */
void initjobs(struct job_t *jobs) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
	clearjob(&jobs[i]);
}

/* maxjid - Returns largest allocated job ID */
int maxjid(struct job_t *jobs) 
{
    int i, max=0;

    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].jid > max)
	    max = jobs[i].jid;
    return max;
}

/* addjob - Add a job to the job list */
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline) 
{
    int i;
    
    if (pid < 1)
	return 0;

    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid == 0) {
	    jobs[i].pid = pid;
	    jobs[i].state = state;
	    jobs[i].jid = nextjid++;
	    if (nextjid > MAXJOBS)
		nextjid = 1;
	    strcpy(jobs[i].cmdline, cmdline);
  	    if(verbose){
	        printf("Added job [%d] %d %s\n", jobs[i].jid, jobs[i].pid, jobs[i].cmdline);
            }
            return 1;
	}
    }
    printf("Tried to create too many jobs\n");
    return 0;
}

/* removejob - Delete a job whose PID=pid from the job list */
int removejob(struct job_t *jobs, pid_t pid) 
{
    int i;

    if (pid < 1)
	return 0;

    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid == pid) {
	    clearjob(&jobs[i]);
	    nextjid = maxjid(jobs)+1;
	    return 1;
	}
    }
    return 0;
}

/* fgpid - Return PID of current foreground job, 0 if no such job */
pid_t fgpid(struct job_t *jobs) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].state == FG)
	    return jobs[i].pid;
    return 0;
}

/* getprocessid  - Find a job (by PID) on the job list */
struct job_t *getprocessid(struct job_t *jobs, pid_t pid) {
    int i;

    if (pid < 1)
	return NULL;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].pid == pid)
	    return &jobs[i];
    return NULL;
}

/* getjobid  - Find a job (by JID) on the job list */
struct job_t *getjobid(struct job_t *jobs, int jid) 
{
    int i;

    if (jid < 1)
	return NULL;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].jid == jid)
	    return &jobs[i];
    return NULL;
}

/* get_jid_from_pid - Map process ID to job ID */
int get_jid_from_pid(pid_t pid) 
{
    int i;

    if (pid < 1)
	return 0;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].pid == pid) {
            return jobs[i].jid;
        }
    return 0;
}

/* showjobs - Print the job list */
void showjobs(struct job_t *jobs) 
{
    int i;
    
    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid != 0) {
	    printf("[%d] (%d) ", jobs[i].jid, jobs[i].pid);
	    switch (jobs[i].state) {
		case BG: 
		    printf("Running ");
		    break;
		case FG: 
		    printf("Foreground ");
		    break;
		case ST: 
		    printf("Stopped ");
		    break;
	    default:
		    printf("showjobs: Internal error: job[%d].state=%d ", 
			   i, jobs[i].state);
	    }
	    printf("%s", jobs[i].cmdline);
	}
    }
}
/******************************
 * end job list helper routines
 ******************************/


/***********************
 * Other helper routines
 ***********************/

/*
 * usage - print a help message
 */
void usage(void) 
{
    printf("Usage: shell [-hvp]\n");
    printf("   -h   print this message\n");
    printf("   -v   print additional diagnostic information\n");
    printf("   -p   do not emit a command prompt\n");
    exit(1);
}

/*
 * unix_error - unix-style error routine
 */
void unix_error(char *msg)
{
    fprintf(stdout, "%s: %s\n", msg, strerror(errno));
    exit(1);
}

/*
 * app_error - application-style error routine
 */
void app_error(char *msg)
{
    fprintf(stdout, "%s\n", msg);
    exit(1);
}

/*
 * Signal - wrapper for the sigaction function
 */
handler_t *Signal(int signum, handler_t *handler) 
{
    struct sigaction action, old_action;

    action.sa_handler = handler;  
    sigemptyset(&action.sa_mask); /* block sigs of type being handled */
    action.sa_flags = SA_RESTART; /* restart syscalls if possible */

    if (sigaction(signum, &action, &old_action) < 0)
	unix_error("Signal error");
    return (old_action.sa_handler);
}

/*
 * sigquit_handler - The driver program can gracefully terminate the
 *    child shell by sending it a SIGQUIT signal.
 */
void sigquit_handler(int sig) 
{
    printf("Terminating after receipt of SIGQUIT signal\n");
    exit(1);
}



