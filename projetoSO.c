/*
* 				  RACE SIMULATOR
*      			 Operating Systems
*
*	By: Etiandro André 2017290285 and João Pedro Dionísio 
*	
*/

/* Para defesa Intermedia falta:
*	-Criação dos processos Gestores de Equipa
*	-Criar threads carro
*	-Envio sincronizado do output para ficheiro de log e ecrã. **
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/msg.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <semaphore.h>
#include <fcntl.h>


#define NUM_TEAMS 3
#define LOGFILE "output.log"
#define STATS "ESTATISTICAS"
#define SHARED_MEM "MEMORIA_PARTILHADA"
#define INPUT_PIPE "input_pipe"
#define CONFIGFILE "config.txt"
#define DEBUG

//--STRUCTS------------------

typedef struct Statistics
{
	int time_units_second;
	int lap_distance;
	int lap_count;
	int team_count;
	int breakdown_check_timer;
	int min_pit;
	int max_pit;
	int fuel_capacity;
}Statistics;

//--GLOBAL VARIABLES---------
Statistics* stats;
pid_t teams[];
pid_t race_simulator, malfunction_manager, race_manager;
pthread_cond_t cond=PTHREAD_COND_INITIALIZER;
pthread_mutex_t mut1=PTHREAD_MUTEX_INITIALIZER;
sem_t *mutex_statistic;
sem_t *mutex_log_file;
int fd_named_pipe;
int mqid;
int id_stat;

//--FUNCTIONS-----------------
int config();
void race_manager();
void signal_sigint();
void handl_sigs();
void malfunction_manager();

int main(){ // Race Simulator
	/*
	:TO-DO:
	1- Read config file **
	2-Create Named Pipe that communicates with the race manager
	3-inicialize race manager process**
	4-inicialize malfunction manager
	5-Signal handling SIGSTP that prints the log and SIGINT to end the race and the program
	must wait for the cars to end the race and then print the log and free every resource
	*/

	int *ar;
	ar = config();

	
	stats->time_units_second = *(ar);
	stats->lap_distance = *(ar +1);
	stats->lap_count = *(ar+2);
	if(*(ar+3)>2){
		stats-> team_count = *(ar+3);
		
	}else{
		printf("Minimum of 3 teams required to start race\nExiting simulator...\n");
		exit(1);
	}

	stats->breakdown_check_timer = *(ar+4);
	stats->min_pit = *(ar+5);
	stats->max_pit = *(ar+6);
	stats->fuel_capacity = *(ar+7);

	fflush(stdout);

	if(signal(SIGINT,SIG_IGN)==SIG_ERR)
	{
		printf("Error: Signal failed!\n");
		exit(EXIT_FAILURE);
	}
	if(signal(SIGUSR1,SIG_IGN)==SIG_ERR)
	{
		printf("Error: Signal failed!\n");
		exit(EXIT_FAILURE);
	}
	printf("[%d]RACE SIMULATOR\n",getpid());

	if(sem_unlink(STATS) == EACCES)
		destroy_everything(5);
	if(sem_unlink(SHARED_MEM) == EACCES)
		destroy_everything(5);
	if(unlink(INPUT_PIPE) == EACCES)
		destroy_everything(7);

	if ((mutex_statistic = sem_open(STATS, O_CREAT | O_EXCL, 0777, 1)) == SEM_FAILED) 
   		destroy_everything(5);
   	if ((mutex_log_file = sem_open(WRITE_LOG, O_CREAT | O_EXCL, 0777, 1)) == SEM_FAILED) 
   		destroy_everything(5);
   	if ((mutex_race_managing_shm = sem_open(SHARED_MEM, O_CREAT | O_EXCL, 0777, 1)) == SEM_FAILED) 
		destroy_everything(5);
	
	project_output_log();

	
   	mqid = msgget(IPC_PRIVATE, IPC_CREAT|0777);
  	if (mqid < 0)
    	destroy_everything(3);
    #ifdef DEBUG
	printf("Message Queue created\n");
	#endif

	creat_shm_statistics();
	#ifdef DEBUG
	printf("Shared for statistics memory created\n");
	#endif
	race_simulator = getpid();
	int p_race_manager,p_malfunction_manager;

	p_race_manager = fork();

	if (p_race_manager < 0)
		destroy_everything(2);
	else if(p_race_manager == 0)
	{
		race_manager();
		exit(0);
	}

	p_malfunction_manager = fork();
	if (p_malfunction_manager < 0)
		destroy_everything(2);
	else if(p_malfunction_manager == 0)
	{
		malfunction_manager();
		exit(0);
	}
	if(signal(SIGINT,signal_sigint)==SIG_ERR)
		destroy_everything(4);



	return 0;
}

int* config (void){
	FILE * fp;
	static int array[0];
	char *token;
	int i = 0;
	fp = fopen("~/Desktop/Projeto_SO/config.txt", "r");
	if(fp == NULL){
		perror("failed: ");
		return 1;
	}
	char buffer[20];
	while(fgets(buffer, 20 -5, fp)){
		buffer[strcspn(buffer, "\n")] = 0;
		token = strtok(buffer,",");
		array[i] = atoi(token);
		i++;
		token = strtok(NULL,",");
		if(token != NULL){
			array[i] = atoi(token);
			i++;
		}
	}
	fclose(fp);
	return array;
}
void log_file_write(char* words)
{
	/*We call this function everytime we want to write something to log
	 we just need to write the string as an argument
	 semaphore to have write sequence so 2 process cant write at the same time and overwrite it
	*/

    if(sem_wait(mutex_log_file)==-1)
		destroy_everything(5);
	FILE *log;
	char acttime[MAX_TIME];
	struct tm *get_time;
	time_t moment = time(0);
    get_time = gmtime (&moment);
	log = fopen(LOGFILE,"a");
	if(log==NULL)
		destroy_everything(8);
    strftime (acttime, sizeof(acttime), "%H:%M:%S ", get_time);
	fprintf(log, "%s%s\n",acttime,words);
	fclose(log);
	printf("%s\n", words);
	if(sem_post(mutex_log_file)==-1)
		destroy_everything(5);
}
void project_output_log()
{
	//Functin that opens logfile for write-only option
	FILE *log;
	log = fopen(LOGFILE,"w");
	if(log==NULL)
		destroy_everything(8);
	fclose(log);
}

void race_manager()
{
	
	race_manager = getpid();
	#ifdef DEBUG
	printf("[%d] Race Manager Process created\n",getpid());
	#endif
	exit() //Remove later
}

void malfunction_manager()
{
	
	malfunction_manager = getpid();
	#ifdef DEBUG
	printf("[%d] Malfunction Manager Process created\n",getpid());
	#endif
	exit() // Remove later
}



void creat_shm_statistics()
{
	//CHANGE STATS CONFIGURATION LATER
	id_stat = shmget(IPC_PRIVATE,sizeof(Statistics),IPC_CREAT|0777);
	if(id_stat <0)
		destroy_everything(1);
	statis = (Statistics*)shmat(id_stat,NULL,0);
	if(statis==(Statistics*)-1)
		destroy_everything(1);
	
}

void signal_sigint()
{
	
	if (getpid() == race_simulator)
	{
		if((fd_named_pipe = open(INPUT_PIPE,O_RDWR)) < 0)
			destroy_everything(7);
	   
		
		char buf[MAX_NAME];
		sprintf(buf,"DRONE_FREE ");
		write(fd_named_pipe,buf,sizeof(buf));
		while(wait(NULL) != -1);
		//write_shm_statistics_terminal();
		
		if(sem_unlink(STATS) == -1)
			destroy_everything(5);
		
		if(sem_unlink(SHARED_MEM) == -1)
			destroy_everything(5);
		if(unlink(INPUT_PIPE) == -1)
			destroy_everything(7);
	
		if(sem_close(mutex_statistic)==-1)
			destroy_everything(5);
		if(sem_close(mutex_race_managing_shm)==-1)
			destroy_everything(5);
		
		if(close(fd_named_pipe)==-1)
			destroy_everything(7);
		
		
		if(shmdt(statis)==-1)
			destroy_everything(1);
		if(shmctl(id_stat,IPC_RMID,NULL)==-1)
			destroy_everything(1);
		if(msgctl(mqid,IPC_RMID,NULL) == -1)
			destroy_everything(3);
		#ifdef DEBUG
		printf("Shared mem free \n");
		#endif
		printf("RACE SIMULATOR[%d] Ended\n",getpid());
		exit(0);
	}
	
	exit(0);
}

void destroy_everything(int n)
{
	switch(n)
	{
		case 1:
			printf("Error in shared memory!\n");
			break;
		case 2:
			printf("Error creating processes\n");
			break;
		case 3:
			printf("Error in message queue\n");
			break;
		case 4:
			printf("Error in signal\n");
			break;
		case 5:
			printf("Error in semaphore\n");
			break;
		case 6:
			printf("Error in threads\n");
			break;
		case 7:
			printf("Error in Pipe\n");
			break;
		case 8:
			printf("Error in log file\n");
			break;
	}

	sem_unlink(STATS);

	sem_unlink(SHARED_MEM);
	unlink(INPUT_PIPE);

	sem_close(mutex_statistic);
	
	close(fd_named_pipe);
	
	shmdt(statis);
	shmctl(id_stat,IPC_RMID,NULL);
	msgctl(mqid,IPC_RMID,NULL);
	printf("Execute kill_ipcs.sh to clean all ipcs");
	system("killall -9 project_exe");
	exit(1);
}

void handl_sigs()
{
	sigemptyset (&block_sigs);
	sigaddset (&block_sigs, SIGHUP);
	sigaddset (&block_sigs, SIGQUIT);
	sigaddset (&block_sigs, SIGILL);
	sigaddset (&block_sigs, SIGTRAP);
	sigaddset (&block_sigs, SIGABRT);
	sigaddset (&block_sigs, SIGFPE);
	sigaddset (&block_sigs, SIGBUS);
	sigaddset (&block_sigs, SIGSEGV);
	sigaddset (&block_sigs, SIGSYS);
	sigaddset (&block_sigs, SIGPIPE);
	sigaddset (&block_sigs, SIGALRM);
	sigaddset (&block_sigs, SIGURG);
	sigaddset (&block_sigs, SIGTSTP);
	sigaddset (&block_sigs, SIGCONT);
	sigaddset (&block_sigs, SIGCHLD);
	sigaddset (&block_sigs, SIGTTIN);
	sigaddset (&block_sigs, SIGTTOU);
	sigaddset (&block_sigs, SIGIO);
	sigaddset (&block_sigs, SIGXCPU);
	sigaddset (&block_sigs, SIGXFSZ);
	sigaddset (&block_sigs, SIGVTALRM);
	sigaddset (&block_sigs, SIGPROF);
	sigaddset (&block_sigs, SIGWINCH);
	sigaddset (&block_sigs, SIGUSR2);
	sigprocmask(SIG_BLOCK,&block_sigs, NULL);
}