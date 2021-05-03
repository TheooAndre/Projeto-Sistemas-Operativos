/* bb
* 				  RACE SIMULATOR
*      			 Operating Systems
*
*	By: Etiandro André 2017290285 and João Pedro Dionísio 2019217030
*
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

#define MAX_TIME 30
#define MAX_NAME 300
#define MAX_LOG_CHAR 600
#define NUM_TEAMS 3// depois remover
#define LOGFILE "output.log"
#define WRITE_LOG "ESCRITA_LOG"
#define STATS "ESTATISTICAS"
#define SHARED_MEM "MEMORIA_PARTILHADA"
#define INPUT_PIPE "input_pipe"
#define CONFIGFILE "config.txt"
#define MAX_CARS 5 //depois remover, cars added in the team manager named pipe
#define DEBUG

//--STRUCTS------------------
typedef struct Data{
	int numcar;
	int time_units_second;
	int lap_distance;
	int lap_count;
	int team_count;
	int breakdown_check_timer;
	int min_pit;
	int max_pit;
	int fuel_capacity;
	int top_5[5];
	int last_place;
}Data;

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
	int top_5[5];
	int last_place;
}Statistics;

typedef struct Car
{

	int car_id;
	int speed;
	int lap_counter;
	int consumption;
	int fuel;
	int fiability;
	int issecurity; // verificar se esta no modo seguranca ou nao(normal)
	int isRacing;
	int isInBox;
	int isquit;
	int isDone;
}Car;

//--GLOBAL VARIABLES---------
Data* data;
Statistics* stats;
pid_t teams[NUM_TEAMS];
pid_t race_simulator, malfunction_manager, race_manager, team_manager;
pthread_t cars[MAX_CARS];
pthread_cond_t cond=PTHREAD_COND_INITIALIZER;
pthread_mutex_t mut1=PTHREAD_MUTEX_INITIALIZER;
sem_t *mutex_statistic;
sem_t *mutex_log_file;
sem_t *mutex_race_managing_shm;
int fd_named_pipe;
int mqid;
int id_stat;
Car car_struct[MAX_CARS];
sigset_t block_sigs;
//--FUNCTIONS-----------------
void config();
void raceManager();
void signal_sigint();
void handl_sigs();
void malfunctionManager();
void teamManager();
void project_output_log();
void log_file_write(char* words);
void *car(void *n);
void creat_shm_statistics();
void handl_sigs();
void destroy_everything(int n);


int main(){ // Race Simulator
	/*
	:TO-DO:
	1- Read config file **
	2-Create Named Pipe that communicates with the race manager
	3-inicialize race manager process**
	4-inicialize malfunction manager**
	5-Signal handling SIGSTP that prints the log and SIGINT to end the race and the program
	must wait for the cars to end the race and then print the log and free every resource
	*/
	printf("[%d]RACE SIMULATOR\n",getpid());
	project_output_log();



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

	if(sem_unlink(STATS) == EACCES)
		destroy_everything(5);

	if(sem_unlink(SHARED_MEM) == EACCES)
		destroy_everything(1);

	/*if(unlink(INPUT_PIPE) == EACCES)
		destroy_everything(7);
		*/
	if(sem_unlink(WRITE_LOG) == EACCES)
		destroy_everything(5);

	if ((mutex_statistic = sem_open(STATS, O_CREAT | O_EXCL, 0700, 1)) == SEM_FAILED)
   		destroy_everything(5);
   	if ((mutex_log_file = sem_open(WRITE_LOG, O_CREAT | O_EXCL, 0700, 1)) == SEM_FAILED)
   		destroy_everything(5);
   	if ((mutex_race_managing_shm = sem_open(SHARED_MEM, O_CREAT | O_EXCL, 0700, 1)) == SEM_FAILED)
		destroy_everything(5);



/*
   	mqid = msgget(IPC_PRIVATE, IPC_CREAT|0777);
  	if (mqid < 0)
    	destroy_everything(3);
    #ifdef DEBUG
	printf("Message Queue created\n");
	#endif
	log_file_write("Message Queue created\n");
*/

	creat_shm_statistics();
	#ifdef DEBUG
	log_file_write("Shared memory for statistics  created\n");
	#endif

	
	race_simulator = getpid();
	int p_race_manager,p_malfunction_manager;

	p_race_manager = fork();
	if (p_race_manager < 0){
		destroy_everything(2);
	}
	else if(p_race_manager == 0)
	{
		raceManager();
		exit(0);
	}

	p_malfunction_manager = fork();
	if (p_malfunction_manager < 0){

		destroy_everything(2);
	}
	else if(p_malfunction_manager == 0)
	{
		malfunctionManager();
		exit(0);
	}
	if(signal(SIGINT,signal_sigint)==SIG_ERR)
		destroy_everything(4);

	
	//signal_sigint();
	return 0;
}



void config (void){
	FILE * fp;
	static int array[0];
	char *token;
	int i = 0;
	fp = fopen("config.txt", "r");
	if(fp == NULL){
		perror("failed: ");

	}

	char buffer[20];
	while(fgets(buffer, 20, fp)){
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
	int *ar;
	ar =  array;
	data->time_units_second = *(ar);
	data->lap_distance = *(ar +1);
	data->lap_count = *(ar+2);  //ver depois
	if(*(ar+3)>2){
		stats-> team_count = *(ar+3);

	}else{
		printf("Minimum of 3 teams required to start race\nExiting simulator...\n");
		log_file_write("Minimum of 3 teams required to start race\nExiting simulator...\n");
		exit(1);
	}

	data->numcar = *(ar+4);
	data->breakdown_check_timer = *(ar+5);
	data->min_pit = *(ar+6);
	data->max_pit = *(ar+7);
	data->fuel_capacity = *(ar+8);
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
	//Function that opens logfile for write-only option
	FILE *log;
	log = fopen(LOGFILE,"w");
	if(log==NULL)
		destroy_everything(8);
	fclose(log);
}

void raceManager()
{
	race_manager = getpid();
	#ifdef DEBUG
	printf("[%d] Race Manager Process created\n",getpid());
	#endif
	log_file_write("Race Manager Process created\n");
	//exit() //Remove later
	printf("%d num_carros teste\n", data->team_count);
	for (int i = 0; i < data->team_count; i++){
		if((teams[i] = fork()) == 0){
			#ifdef DEBUG
			printf("TEAM MANAGER STARTED\n");
			#endif
			log_file_write("TEAM MANAGER STARTED\n");
			teamManager();
			exit(0);

		}
		else if(teams[i] == -1){
			destroy_everything(2);
		}
	}
}


void teamManager()
{
	char buf[512];
	int fd_named_pipe, n;
	//CRIAR THREADS CARRO
	team_manager = getpid();
	#ifdef DEBUG
	printf("[%d] Team Manager Process created\n",getpid());
	#endif
	//Adiciona os carros de acordo aos comandos da named pipe!
	if((mkfifo(INPUT_PIPE,O_CREAT|0600)<0) && errno!=EEXIST)
		destroy_everything(7);
	if((fd_named_pipe = open(INPUT_PIPE,O_RDWR)) < 0)
		destroy_everything(7);

	while( (n = read(fd, buf, 512) ) > 0) {

        	if ( write(STDOUT_FILENO, buf, n) != n) { 
            		destroy_everything(7);
        	}
    	}
	close(fd);
	
	
	char team;
	int values [4];
	char modelo[] = "ADDCAR TEAM:";
	char aux[512];
	int first = 1;
	int aux = 0;
	int y = 0;
	int b = 1;
	int x = 0;
	while(b){
	if(strcmp(buf,"START RACE!") == 0){
		b = false;
	}
	for(int i=0; i< strlen(buf);; i++){
		if(x == 1){
			if(buf[i] == ',' || buf[i] == '\n'){
				if(first == 1){
					team = aux[0];
				}else{
					for(i=0; i < strlen(aux); i++){
						int values[aux2] = values[aux2] * 10 + ( num[i] - '0' );
					}
					aux2++;
				}
				aux = 0;
				memset(buf,0,strlen(buf));
				x = 0;
				
			}
			aux[aux] = buf[i];
			aux++;
		}
		if(y == 1 && buf[i] == ' '){
			x = 1;
			y = 0;
		}
		if(buf[i] == ':')
			y = 1;
		
	}
	}

	sem_wait(mutex_race_managing_shm);
	printf("%d num_carros teste\n", data->num_carros);
	for (int i = 0; i < data->num_carros; ++i)
	{

		/*
		int lap_count;
		int fuel_capacity;
		bool issecurity = false; // verificar se esta no modo seguranca ou nao(normal)
		bool isRacing = false;
		bool isInBox = false;
		bool isEmpty = false;
		bool isDone = false;
	*/
		car_struct[i].lap_count=0;
		car_struct[i].car_id=i;
		//car_struct[i].state = 1; // we can do states like this or with booleans, ill leave this here so i can think better later
		car_struct[i].fuel_capacity=1;
		car_struct[i].issecurity= 0;
		car_struct[i].isRacing = 1;
		car_struct[i].isInBox= 0;
		car_struct[i].isEmpty= 0;
		car_struct[i].isDone= 0;

		if (pthread_create(&cars[i], NULL, car,&car_struct[i])!=0)
			destroy_everything(6);
		usleep(20);
	}
	#ifdef DEBUG
	printf("All cars created\n");
	#endif
	sem_post(mutex_race_managing_shm);
	exit(0); //Remove later
}


void *car(void *n)
{
	#gravar os log
	Car car = *((Car *)n);
	#ifdef DEBUG
	int o = 0;
	printf("[%d]Car created\n",car.car_id);
	for(int i = 0; i< data.lap_count; i++){
		if (car.isquit == 1){
			break;
		}
		o = 0;
		if((car.issecurity == 1 || car.requestbox == 1)&& memoriapartilhadaboxislivre){
			car.isRacing = 0;
			car.isInBox = 1;
			setmemoriapartilhadaboxischeia;
			int randnum = (rand() %(max_pit - min_pit + 1)) + min_pit;
			sleep(randnum);
			sleep(2);
			car.fuel = data.fuel_capacity;
			car.requestbox = 0;
			car.issecurity = 0;
			car.isInBox = 1;
			car.isInBox = 1;
			car.isRacing = 1;
			car.requestbox = 0;
		}
		while(o < lap_distance){
			if (car.fuel < car.consumption){
				car.isquit = 1;
			}else if(car.fuel <= ((lap_distance/car.speed) * car.consumption) * 4){
				car.requestbox = 1;
				if(car.fuel <= ((lap_distance/car.speed) * car.consumption) * 2){
					car.issecurity = 1;
					setmemoriapartilhadacarrosecurity;
				}
			}
			sleep(time_units_second);
			o += speed;
			car.fuel -= car.consumption;
		}
		car.lap_counter += 1;
	}
	if (car.isquit == 1){
		#grava no log
		printf("gave up");
	}else{
		for(int i = 0; i<5, i++){
			if(!top_5[i]){
				top_5[i] = car.car_id;  
			}
		}
		car.isDone = 1;
	}
	#endif
	pthread_exit(NULL);
}

void malfunctionManager()
{

	malfunction_manager = getpid();
	#ifdef DEBUG
	printf("[%d] Malfunction Manager Process created\n",getpid());
	while(1){
		sleep(data->breakdown_check_timer);
		for(int i = 0; i < numcar; i++){
			if(car_struct[i]->isRacing == 1){
				int randnum = (rand() %(100 - 1 + 1)) + 1;
				if (randnum > car_struct[i]->fiability){
					car_struct[i]->issecurity = 1;
					car_struct[i]->consumption *= 0.4 # change variable to consumption;
					car_struct[i]->speed *= 0.3;
				}
			}
		}
	}
	#endif
	exit(0); // Remove later
}



void creat_shm_statistics()
{

	//CHANGE STATS CONFIGURATION LATER
	id_stat = shmget(IPC_PRIVATE,sizeof(Statistics),IPC_CREAT|0777);
	if(id_stat <0){
		printf("<0");
		destroy_everything(1);
		}
	stats = (Statistics*)shmat(id_stat,NULL,0);
	if(stats==(Statistics*)-1){
		destroy_everything(1);
		}
	stats = (Statistics*)malloc(sizeof(Statistics));

	fflush(stdout);
}

void signal_sigint()
{

	if (getpid() == race_simulator)
	{
		
		int i = 0;
		/*char buf[MAX_NAME];
		sprintf(buf,"CAR FREE ");
		write(fd_named_pipe,buf,sizeof(buf));*/
		kill(malfunction_manager,SIGKILL);
		kill(race_manager,SIGKILL);
		kill(team_manager,SIGKILL);
		while (i < (NUM_TEAMS))
		kill(teams[i++], SIGKILL);
		while(wait(NULL) != -1);

		//write_shm_statistics_terminal();
		if(sem_unlink(STATS) == -1)
			destroy_everything(5);
		if(sem_unlink(WRITE_LOG) == -1)
			destroy_everything(5);
		if(sem_unlink(SHARED_MEM) == -1)
			destroy_everything(5);
		if(unlink(INPUT_PIPE) == -1)
			destroy_everything(7);

		if(sem_close(mutex_log_file)==-1)
			destroy_everything(5);

		if(sem_close(mutex_statistic)==-1)
			destroy_everything(5);

		if(sem_close(mutex_race_managing_shm)==-1)
			destroy_everything(5);
		
		if(close(fd_named_pipe)==-1)
			destroy_everything(7);

	


		if(shmdt(stats)==-1)
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
	sem_unlink(WRITE_LOG);		
	sem_unlink(SHARED_MEM);

	unlink(INPUT_PIPE);
	sem_close(mutex_statistic);
	sem_close(mutex_log_file);
	sem_close(mutex_race_managing_shm);


	close(fd_named_pipe);
	shmdt(stats);
	shmctl(id_stat,IPC_RMID,NULL);
	msgctl(mqid,IPC_RMID,NULL);
	printf("Execute kill_ipcs.sh to clean all ipcs");
	system("killall -9 a");
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
