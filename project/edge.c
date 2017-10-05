/**
 * edge.c
 *
 * Receives bitwise and/bitwise or operation jobs from clients, sends jobs to
 * backend AND/backend OR servers, and  sends the results to the appropriate
 * client.
 *
 * Usage: ./edge
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#define CLIENT_RECV_BYTES 29 // number of bytes received from client
#define BACKEND_SEND_BYTES 29 // number of bytes send to backend server
#define BACKEND_RECV_BYTES 14 // number of bytes received from backend server
#define CLIENT_SEND_BYTES 10 // number of bytes sent to client

#define OPERATOR_BYTES 3 // maximum number of bytes used by operator
#define OPERAND_BYTES 10 // maximum number of bytes used by operand
#define RESULT_BYTES 10 // maximum number of bytes used by result

#define EDGE_IP "127.0.0.1" // edge server IPv4 address
#define DGRAM_PORT 24926 // datagram socket port number
#define WELCOME_PORT 23926 // welcoming stream socket port number
#define BACKLOG 5 // buffer size for welcoming stream socket

#define AND_IP "127.0.0.1" // and server IPv4 address
#define AND_PORT 22926 // and server port number

#define OR_IP "127.0.0.1" // or server IPv4 address
#define OR_PORT 21926 // or server port number

/**
 * struct to store job data
 */
struct job {
   char operator[OPERATOR_BYTES + 1];  
   char operand1[OPERAND_BYTES + 1];
   char operand2[OPERAND_BYTES + 1];
   char result[RESULT_BYTES + 1]; 
};

/**
 * setupdgramsock creates a datagram socket and binds it.
 * @return int socket descriptor, -1 if unsuccessful
 */
int setupdgramsock();

/**
 * setupwelcstreamsock creates a welcoming stream socket, binds it, and listens
 * for incoming connections.
 * @return int socket descriptor, -1 if unsuccessful
 */
int setupwelcstreamsock();

/**
 * sigchldhandler is used by reapzombproc to reap zombie processes.
 * @param s int signal descriptor
 */
void sigchldhandler(int s);

/**
 * reapzombieproc reaps zombie processes created when child processes exit.
 * @return int 0 if successful, 1 if unsuccessful
 */
int reapzombproc();

/**
 * recvjob receives a job from a client.
 * @param connect_sd int connected stream socket descriptor
 * @param job_ptr pointer to struct job
 * @param num_and_jobs_ptr pointer to int number of AND jobs
 * @param num_or_jobs_ptr pointer to int number of OR jobs
 * @return int number of jobs, -1 if unsuccessful
 */
int recvjob(int connect_sd, struct job * job_ptr, int * num_and_jobs_ptr,
   int * num_or_jobs_ptr);

/**
 * sendjobs sends a client's jobs to the backend servers.
 * @param dgram_sd int datagram socket descriptor
 * @param and_addr_ptr pointer to backend AND server socket address
 * @param or_addr_ptr pointer to backend OR server socket address
 * @param jobs array of jobs
 * @param num_jobs int number of jobs
 * @param num_and_jobs int number of AND jobs
 * @param num_or_jobs int number of OR jobs
 * @return int 0 if successful, 1 if unsuccessful
 */
int sendjobs(int dgram_sd, struct sockaddr_in * and_addr_ptr,
   struct sockaddr_in * or_addr_ptr, struct job jobs[], int num_jobs,
   int num_and_jobs, int num_or_jobs);

/**
 * recvresults receives the results from a backend server.
 * @param dgram_sd int datagram socket descriptor
 * @param backend_addr_ptr pointer to backend server socket address
 * @param backend_addr_len socklen_t length of socket address
 * @param num_backend_jobs int number of backend jobs
 * @param jobs array of jobs
 * @return int 0 if successful, 1 if unsuccessful
 */
int recvresults(int dgram_sd, struct sockaddr_in * backend_addr_ptr,
   socklen_t backend_addr_len, int num_backend_jobs, struct job jobs[]);

/**
 * sendresults sends the results to the client.
 * @param connect_sd int connected stream socket descriptor
 * @param num_jobs int number of jobs
 * @param jobs array of jobs
 * @return int 0 if successful, 1 if unsuccessful
 */
int sendresults(int connect_sd, int num_jobs, struct job jobs[]);

/**
 * main
 * sockets are setup, jobs are received from clients, the jobs are sent to
 * backend servers, the results are received and sent back to the client.
 * @param argc int number of arguments
 * @param argv pointer to array of c string argument pointers
 * @return int 0 if successful, 1 if unsuccessful
 */
int main(void)
{
   // Setup datagram socket
   int dgram_sd;
   if ((dgram_sd = setupdgramsock()) == -1)
   {
      return EXIT_FAILURE;
   }

   // Setup welcoming stream socket
   int welcome_sd;
   if ((welcome_sd = setupwelcstreamsock()) == -1)
   {
      close(dgram_sd);
      return EXIT_FAILURE;
   }

   // Reap zombie processes created as child processes exit
   if (reapzombproc() == EXIT_FAILURE)
   {
      close(dgram_sd);
      close(welcome_sd);
      return EXIT_FAILURE;
   }

   // Specify AND server address information
   struct sockaddr_in and_addr;
   and_addr.sin_family = AF_INET;
   and_addr.sin_port = htons(AND_PORT); // store in network byte order
   and_addr.sin_addr.s_addr = inet_addr(AND_IP);
   memset(and_addr.sin_zero, '\0', sizeof(and_addr.sin_zero)); // allows for
      //typecasting
   socklen_t and_addr_len = sizeof(and_addr);

   // Specify OR server address information
   struct sockaddr_in or_addr;
   or_addr.sin_family = AF_INET;
   or_addr.sin_port = htons(OR_PORT); // store in network byte order
   or_addr.sin_addr.s_addr = inet_addr(OR_IP);
   memset(or_addr.sin_zero, '\0', sizeof(or_addr.sin_zero)); // allows for
      //typecasting
   socklen_t or_addr_len = sizeof(or_addr);

   struct sockaddr_storage client_addr;  
   socklen_t client_addr_len = sizeof(client_addr);
   int connect_sd;

   // Print message indicating edge server is up and running
   fprintf(stdout, "The edge server is up and running.\n");

   while (1)
   {
      // Accept incoming connections from clients
      if ((connect_sd = accept(welcome_sd, (struct sockaddr *) &client_addr,
         &client_addr_len)) == -1)
      {
         fprintf(stderr, "ERROR: Failed to accept client connection.\n");
         continue;
      }

      if (!fork())
      {
         // Child process
         close(welcome_sd);

         // Receive initial job from client to get number of jobs
         struct job job0;         
         int num_jobs;
         int num_and_jobs = 0;
         int num_or_jobs = 0;
         
         if ((num_jobs = recvjob(connect_sd, &job0, &num_and_jobs,
            &num_or_jobs)) == -1)
         {
            close(connect_sd);
            exit(EXIT_FAILURE);
         }

         struct job jobs[num_jobs];
         jobs[0] = job0;

         // Receive remaining jobs from client
         if (num_jobs > 1)
         {
            for (int i = 1; i < num_jobs; i++)
            {
               if (recvjob(connect_sd, &jobs[i], &num_and_jobs, &num_or_jobs)
                  == -1)
               {
                  close(connect_sd);
                  exit(EXIT_FAILURE);
               }              
            }
         }

         // Print message indicating edge server has received jobs from client
         fprintf(stdout, "The edge server has received %d jobs from the client"
            " using TCP over port %d.\n", num_jobs, WELCOME_PORT);

         // Send jobs to backend servers
         if (sendjobs(dgram_sd, &and_addr, &or_addr, jobs, num_jobs,
            num_and_jobs, num_or_jobs) == EXIT_FAILURE)
         {
            close(connect_sd);
            exit(EXIT_FAILURE);
         }

         // Receive results from AND server
         if (recvresults(dgram_sd, &and_addr, and_addr_len, num_and_jobs, jobs)
            == EXIT_FAILURE)
         {
            close(connect_sd);
            exit(EXIT_FAILURE);
         }

         // Receive jobs from OR server
         if (recvresults(dgram_sd, &or_addr, or_addr_len, num_or_jobs, jobs)
            == EXIT_FAILURE)
         {
            close(connect_sd);
            exit(EXIT_FAILURE);
         }

         // Print message indicating edge server has started receiving results
            //from the backend servers
         fprintf(stdout, "The edge server has started receiving the computation"
            " results from the backend AND server and the backend OR server"
            " using UDP over port %d.\nThe computation results are:\n",
            DGRAM_PORT);

         // Print computation results
         for (int i = 0; i < num_jobs; i++)
         {
            fprintf(stdout, "%s %s %s = %s\n", jobs[i].operand1,
               jobs[i].operator, jobs[i].operand2, jobs[i].result);
         }

         // Print message indicating edge server has received all results
         fprintf(stdout, "The edge server has successfully finished receiving"
             " all computation results from the backend AND server and the"
             " backend OR server.\n");
         
         // Send results to client
         if (sendresults(connect_sd, num_jobs, jobs) == EXIT_FAILURE)
         {
            close(connect_sd);
            exit(EXIT_FAILURE);
         }
         
         close(connect_sd);
         exit(EXIT_FAILURE);
      }
      else
      {
         // Parent process
         close(connect_sd);
      }
   }

	return 0;
}

int setupdgramsock()
{
   // Create datagram socket
   int dgram_sd;

   if ((dgram_sd = socket(PF_INET, SOCK_DGRAM, 0)) == -1)
   {
      fprintf(stderr, "ERROR: Failed to create datagram socket.\n");
      return -1;
   }

   // Allow datagram socket to reuse port
   int yes = 1;

   if (setsockopt(dgram_sd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
   {
      fprintf(stderr, "ERROR: setsockopt for datagram socket failed.\n");
      close(dgram_sd);
      return -1;
   }

   // Specify datagram socket address information
   struct sockaddr_in dgram_addr;
   dgram_addr.sin_family = AF_INET;
   dgram_addr.sin_port = htons(DGRAM_PORT); // store in network byte order
   dgram_addr.sin_addr.s_addr = inet_addr(EDGE_IP);
   memset(dgram_addr.sin_zero, '\0', sizeof(dgram_addr.sin_zero)); // allows for
      //typecasting 

   // Bind datagram socket to address
   if ((bind(dgram_sd, (struct sockaddr *) &dgram_addr, sizeof(dgram_addr)))
      == -1)
   {
      fprintf(stderr, "ERROR: Failed to bind datagram socket.\n");
      close(dgram_sd);
      return -1;
   }

   return dgram_sd;
}

int setupwelcstreamsock()
{
   // Create welcoming stream socket  
   int welcome_sd;

   if ((welcome_sd = socket(PF_INET, SOCK_STREAM, 0)) == -1)
   {
      fprintf(stderr, "ERROR: Failed to create welcoming stream socket.\n");
      return -1;
   }
   
   // Allow welcoming socket to reuse port
   int yes = 1;

   if (setsockopt(welcome_sd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))
         == -1)
   {
      fprintf(stderr, "ERROR: setsockopt for welcoming stream socket"
         " failed.\n");
      close(welcome_sd);
      return -1;
   }

   // Specify welcoming stream socket address information
   struct sockaddr_in welcome_addr;
   welcome_addr.sin_family = AF_INET;
   welcome_addr.sin_port = htons(WELCOME_PORT); // store in network byte order
   welcome_addr.sin_addr.s_addr = inet_addr(EDGE_IP);
   memset(welcome_addr.sin_zero, '\0', sizeof(welcome_addr.sin_zero)); //
      //allows for typecasting
   
   // Bind welcoming stream socket to address
   if (bind(welcome_sd, (struct sockaddr *) &welcome_addr,
      sizeof(welcome_addr)) == -1)
   {
      fprintf(stderr, "ERROR: Failed to bind welcoming stream socket.\n");
      close(welcome_sd);
      return -1;
   }

   // Listen on welcoming stream socket for incomming connections
   if (listen(welcome_sd, BACKLOG) == -1)
   {
      fprintf(stderr, "ERROR: Failed to listen on welcoming stream socket.\n");
      close(welcome_sd);
      return -1;
   }

   return welcome_sd;
}

void sigchldhandler(int s)
{
	// waitpid() can overwrite errno, so it is saved and restored
	int saved_errno = errno;

	while (waitpid(-1, NULL, WNOHANG) > 0);

	errno = saved_errno;
}

int reapzombproc()
{
   struct sigaction sa;
   sa.sa_handler = sigchldhandler;
   sigemptyset(&sa.sa_mask);
   sa.sa_flags = SA_RESTART;

   if (sigaction(SIGCHLD, &sa, NULL) == -1)
   {
      fprintf(stderr, "ERROR: sigaction failed.\n");
      return EXIT_FAILURE;
   }

   return EXIT_SUCCESS;
}

int recvjob(int connect_sd, struct job * job_ptr, int * num_and_jobs_ptr,
   int * num_or_jobs_ptr)
{
   char buffer[CLIENT_RECV_BYTES + 1];

   if (recv(connect_sd, &buffer, CLIENT_RECV_BYTES, 0) != CLIENT_RECV_BYTES)
   {
      fprintf(stderr, "ERROR: Failed to receive job from client.\n");
      return -1;
   }
   buffer[CLIENT_RECV_BYTES] = '\0'; // append null character to buffer

   int num_jobs;

   // Extract data from client message
   if (sscanf(buffer, "%s %s %s %d", job_ptr->operator, job_ptr->operand1,
      job_ptr->operand2, &num_jobs) != 4)
   {
      fprintf(stderr, "ERROR: Failed to extract fields from job message.\n");
      return -1;
   }

   if (strcmp(job_ptr->operator, "and") == 0)
   {
      (*num_and_jobs_ptr)++;
   }
   else if (strcmp(job_ptr->operator, "or") == 0)
   {
      (*num_or_jobs_ptr)++;
   }
   else
   {
      fprintf(stderr, "ERROR: Invalid operator received from client.\n");
      return -1;
   }

   return num_jobs;
}

int sendjobs(int dgram_sd, struct sockaddr_in * and_addr_ptr,
   struct sockaddr_in * or_addr_ptr, struct job jobs[], int num_jobs,
   int num_and_jobs, int num_or_jobs)
{
   for (int i = 0; i < num_jobs; i++)
   {
      char payload[BACKEND_SEND_BYTES + 1];

      if (strcmp(jobs[i].operator, "and") == 0)
      {
         sprintf(payload, "%10s %10s %3d %3d", jobs[i].operand1,
            jobs[i].operand2, i, num_and_jobs);

         if (sendto(dgram_sd, payload, strlen(payload), 0,
            (struct sockaddr *) and_addr_ptr, sizeof(*and_addr_ptr))
            != BACKEND_SEND_BYTES)
         {
            fprintf(stderr, "ERROR: Failed to send job to backend AND"
               " server.\n");
            return EXIT_FAILURE;
         }
      }
      else if (strcmp(jobs[i].operator, "or") == 0)
      {
         sprintf(payload, "%10s %10s %3d %3d", jobs[i].operand1,
            jobs[i].operand2, i, num_or_jobs);

         if (sendto(dgram_sd, payload, strlen(payload), 0,
            (struct sockaddr *) or_addr_ptr, sizeof(*or_addr_ptr))
            != BACKEND_SEND_BYTES)
         {
            fprintf(stderr, "ERROR: Failed to send job to backend OR"
               " server.\n");
            return EXIT_FAILURE;
         }
      }
      else
      {
         fprintf(stderr, "ERROR: Invalid operator.\n");
         return EXIT_FAILURE;
      }
   }

   // Print messages indicating that jobs were sent to the backend servers
   fprintf(stdout, "The edge server has successfully sent %d lines to the"
      " backend AND server.\n", num_and_jobs);
   fprintf(stdout, "The edge server has successfully sent %d lines to the"
      " backend OR server.\n", num_or_jobs);

   return EXIT_SUCCESS;
}

int recvresults(int dgram_sd, struct sockaddr_in * backend_addr_ptr,
   socklen_t backend_addr_len, int num_backend_jobs, struct job jobs[])
{
   for (int i = 0; i < num_backend_jobs; i++)
   {
      char buffer[BACKEND_RECV_BYTES + 1];
      int and_addr_len;

      if (recvfrom(dgram_sd, buffer, BACKEND_RECV_BYTES, 0,
         (struct sockaddr *) backend_addr_ptr, &backend_addr_len)
         != BACKEND_RECV_BYTES)
      {
         fprintf(stderr, "ERROR: Failed to receive result from backend AND"
            " server.\n");
         return EXIT_FAILURE;
      }
      buffer[BACKEND_RECV_BYTES] = '\0'; // append null character to buffer

      int job_number;
      char result[11];

      if (sscanf(buffer, "%d %s", &job_number, result) != 2)
      {
         fprintf(stderr, "ERROR: Failed to extract fields from result\n.");
         return EXIT_FAILURE;
      }

      strcpy(jobs[job_number].result, result);
   }

   return EXIT_SUCCESS;
}

int sendresults(int connect_sd, int num_jobs, struct job jobs[])
{
   char payload[CLIENT_SEND_BYTES + 1];

   for (int i = 0; i < num_jobs; i++)
   {
      sprintf(payload, "%10s", jobs[i].result);

      if (send(connect_sd, payload, strlen(payload), 0) != CLIENT_SEND_BYTES)
      {
         fprintf(stderr, "ERROR: Failed to send result to client.\n");
         return EXIT_FAILURE;
      }
   }

   // Print message indicating edge server has sent all results to the client
   fprintf(stdout, "The edge server has successfully finished sending all"
      " computation results to the client.\n");

   return EXIT_SUCCESS;
}

