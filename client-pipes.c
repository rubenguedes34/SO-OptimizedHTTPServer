#include "stdio.h"
#include "stdlib.h"
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "string.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <stdbool.h>
#define BUFSIZE 8096
#define PORT 8080
#define IP "127.0.0.1"

#define TIMER_START() gettimeofday(&tv1, NULL)
#define TIMER_STOP() \
gettimeofday(&tv2, NULL); \
timersub(&tv2, &tv1, &tv); \
time_delta = (float)tv.tv_sec + tv.tv_usec / 1000000.0
bool graceful_shutdown = false;
static volatile sig_atomic_t sigint_received = 0;

typedef struct results {
    int pid;
    int batch_number;
    int batch_sequence;
    float total_time;
    float min_time;
    float max_time;
}RESULTS;

int pexit_3(char * msg)
{
    perror(msg);
    exit(1);
}

void sigint_handler(int sig) {
    graceful_shutdown = true;
    sigint_received = 1;
    if(sigint_received>1) exit(0);
}

int main(int argc, char * argv[]){
    int i,sockfd;
    char buffer[BUFSIZE];
    static struct sockaddr_in serv_addr;
    struct timeval tv1, tv2, tv;
    float time_delta;

    /**numero de pedidos**/
    int N = atoi(argv[1]);
    /** numero de filhos**/
    int M = atoi(argv[2]);
    //printf("Teste: N = %d M = %d\n", N,M);
    printf("client connected to IP = %s PORT = %d\n\n",IP,PORT);
    //sprintf(buffer,"GET /%s HTTP/1.0 \r\n\r\n", argv[3]);

    struct sigaction act;
    act.sa_handler = sigint_handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGINT, &act, NULL);
    
    int fds[2];
    if (pipe(fds) == -1) {
        perror("pipe() failed");
        exit(1);
    }
    int total_requests_completed = 0;
    int n_batches = (N/M);
    RESULTS resultspai[M][n_batches];			//1 posição para cada filho criado
    memset(resultspai, 0, sizeof(resultspai));	//iniciar as pos com 0's
    
    //int next_batch = 1;

    pid_t pid[M];
    for (int i = 0; i < M; i++) {	// M filhos
        pid[i] = fork();
        if (pid[i] < 0) {
            perror("fork() failed");
            exit(1);
        }
        if (pid[i] == 0) {
            close(fds[0]);	//close leitura
            RESULTS results[n_batches]; // array for each child
            memset(results, 0, sizeof(results));	//iniciar as pos com 0's

            for (int j = 1; j <= n_batches; j++) {
            
            
                TIMER_START();
                if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
                    pexit_3("socket() failed");
                serv_addr.sin_family = AF_INET;
                serv_addr.sin_addr.s_addr = inet_addr(IP);
                serv_addr.sin_port = htons(PORT);
                if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
                    	if (graceful_shutdown) exit(1);
                    	else pexit_3("connect() failed");
                //printf("Send bytes=%d %s\n", (int) strlen(buffer), buffer);
                write(sockfd, buffer, strlen(buffer));
                close(sockfd); // Close socket after each request
                //while ((i = read(sockfd, buffer, BUFSIZE)) > 0)
                //write(1, buffer, i);
                TIMER_STOP();

                // save in array
                results[j-1].pid = getpid();			//initial j=1, but using pos 0 of array
                results[j-1].batch_number = j;
                results[j-1].batch_sequence = i;
                results[j-1].total_time = time_delta;
                results[j-1].min_time = time_delta;
                results[j-1].max_time = time_delta;

		if (!graceful_shutdown) {
                    RESULTS resultpipe = { getpid(),j, i, time_delta, time_delta,time_delta };	
                	if (write(fds[1], &resultpipe, sizeof(resultpipe)) < 0) {
                    	    perror("write() failed");
                    	    exit(1);
                	}
                }
		// copy results to parent process array
                memcpy(resultspai, results, sizeof(results));
                printf("pid:%d;\tbatch-number:%d;\tbatch-sequence:%d;\ttime:%fs\n", getpid(), j, i, time_delta);
                total_requests_completed++;
            }

            close(fds[1]);	//close escrita
            exit(0);
        }

    }//ALl requests done!


    //Parent
    int fd = open("files/pipes.txt", O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }
    
    close(fds[1]);
    int status;
    for (int i = 0; i < M; i++) {
        waitpid(pid[i], &status, 0);
    }
    if (!graceful_shutdown) printf("All child processes terminated successfully\n\n");

    //SIGINT received - terminate child + print estatisticas dos valores obtidos até aquele momento + print resultados obtidos file
    if (graceful_shutdown) {
        printf("Received SIGINT signal.\n");
        for (int i = 0; i < M; i++) {		//terminate all childs
            kill(pid[i], SIGTERM);
        }    
        printf("All child processes forced to terminate.\n\n");

	//retirar timers do pipe para colocar array resultspai + escrever file todos resultados obtidos até sigint
	ssize_t read_bytes;
    	    for (int m = 0; m< M; m++) {
                for (int n=0; n< n_batches; n++) {
                    RESULTS resultpipe;
            	    while ((read_bytes = read(fds[0], &resultpipe, sizeof(resultpipe))) > 0){
                	resultspai[m][n] = resultpipe;		//ex. 300 3 =>	[0][1]...[2][100]
                	char buffer[BUFSIZE];
                	sprintf(buffer, "pid:%d;batch_number:%d;batch_sequence:%d;time:%f\n",resultpipe.pid,resultpipe.batch_number,resultpipe.batch_sequence,resultpipe.total_time);
               		write(fd, buffer, strlen(buffer));
                	n++;	//resultspai[m][++] untill it finishes all the results for each batch
                	total_requests_completed++;
            	    }
        	}
    	    }

        //Relatório com estatisticas até sigint
    	double total_time = 0;
    	double min_time = resultspai[0][0].min_time;
   	double max_time = resultspai[0][0].max_time;
   	double avg_time_per_request = 0;
    	int num_requests = total_requests_completed;
   	for (int i = 0; i < M; i++) {
      	  for (int j = 0; j < n_batches; j++) {
            	total_time += resultspai[i][j].total_time;
            	if (resultspai[i][j].min_time < min_time) {
                	min_time = resultspai[i][j].min_time;
            	}
           	 if (resultspai[i][j].max_time > max_time) {
                	max_time = resultspai[i][j].max_time;
            	}
          }
    	}

    	if (num_requests > 0) {
       		avg_time_per_request = total_time / num_requests;
    	}
    	printf("Relatório de execução dos pedidos atendidos:\n");
    	printf("Tempo total dos pedidos: %f\n", total_time);
    	printf("Tempo mínimo dos pedidos: %f\n", min_time);
    	printf("Tempo máximo dos pedidos: %f\n", max_time);
    	printf("Tempo médio por pedido: %f\n", avg_time_per_request);
    	exit(0);
    } 
    
    
    ssize_t read_bytes;
    for (int m = 0; m< M; m++) {
        for (int n= 0; n< n_batches; n++) {
            RESULTS resultpipe;
            while ((read_bytes = read(fds[0], &resultpipe, sizeof(resultpipe))) > 0){
                resultspai[m][n] = resultpipe;		//ex. 300 3 =>	[0][1]...[2][100]
                    char buffer[BUFSIZE];
                sprintf(buffer, "pid:%d;batch_number:%d;batch_sequence:%d;time:%f\n",resultpipe.pid,resultpipe.batch_number,resultpipe.batch_sequence,resultpipe.total_time);
                write(fd, buffer, strlen(buffer));
                n++;	//resultspai[m][++] untill it finishes all the results for each batch
                total_requests_completed++;
            }

        }

    }
    close(fds[0]);
    close(fd);


    //Relatório final antes programa terminar
    double total_time = 0;
    double min_time = resultspai[0][0].min_time;
    double max_time = resultspai[0][0].max_time;
    double avg_time_per_request = 0;
    for (int i = 0; i < M; i++) {
        for (int j = 0; j < n_batches; j++) {
            total_time += resultspai[i][j].total_time;
            if (resultspai[i][j].min_time < min_time) {
                min_time = resultspai[i][j].min_time;
            }
            if (resultspai[i][j].max_time > max_time) {
                max_time = resultspai[i][j].max_time;
            }
        }
    }
    avg_time_per_request = total_time / N;
    
    printf("Relatório de execução dos pedidos:\n");
    printf("Tempo total dos pedidos: %f\n", total_time);
    printf("Tempo mínimo dos pedidos: %f\n", min_time);
    printf("Tempo máximo dos pedidos: %f\n", max_time);
    printf("Tempo médio por pedido: %f\n", avg_time_per_request);
  
    return 0;
}
