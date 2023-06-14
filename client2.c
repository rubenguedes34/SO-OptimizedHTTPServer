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
}

int main(int argc, char * argv[]){
    int i,sockfd;
    char buffer[BUFSIZE];
    static struct sockaddr_in serv_addr;
    struct timeval tv1, tv2, tv;
    float time_delta;
    int status;

    /**numero de pedidos**/
    int N = atoi(argv[1]);
    /** numero de filhos**/
    int M = atoi(argv[2]);
    printf("Teste: N = %d M = %d\n\n", N,M);
    printf("client connected to IP = %s PORT = %d\n\n",IP,PORT);
    //sprintf(buffer,"GET /%s HTTP/1.0 \r\n\r\n", argv[3]);

    struct sigaction act;
    act.sa_handler = sigint_handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGINT, &act, NULL);
    
    int n_batches = (N/M);
    pid_t pid[M];
    int fd = open("files/output.txt", O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd == -1) {
   	perror("open");
   	exit(EXIT_FAILURE);
    }
    
    
    for (i = 1; i <= n_batches; i++) {		//ex. ./client 300 10  i=> 1-30

        for (int j = 0; j < M; j++) {			//j=> 0-9
        	pid[j] = fork();
        	
            if (pid[j] == 0) {
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
                //    write(1, buffer, i);
                TIMER_STOP();

		if (!graceful_shutdown) {
		    // write the result to the output file
    		    char buffer[100];
    		    sprintf(buffer, "pid:%d;batch-number:%d;batch-sequence:%d;time%f\n",getpid(),i,j,time_delta);
    		    write(fd, buffer, strlen(buffer));
            	}
                printf("pid:%d;\tbatch-number:%d; \tbatch-sequence:%d;\ttime:%fs\n", getpid(), i, j, time_delta);
                exit(0);
            }

        }
        
        // wait for all child processes in this batch to finish
    	for (int l = 0; l < M; l++) {
		waitpid(pid[l], &status, 0);
    	}	
    	printf("All child processes in batch %d terminated successfully\n\n", i);
    	char paragrafo[10]="\n";
    	write(fd,paragrafo, strlen(paragrafo));
    	
    	 //SIGINT signal received
    	if (graceful_shutdown) {
            printf("Received SIGINT signal.\n");
            // Terminate all child processes
            for (int i = 0; i < M; i++) {
                kill(pid[i], SIGTERM);
            }  
            printf("All child processes forced to terminate...\n\n");  
        	
            //Relatorios
            
            
            exit(0);
    	}
    
    
    }//ALl requests done
    close(fd);
    

    return 0;
}

