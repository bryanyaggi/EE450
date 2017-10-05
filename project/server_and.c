/**
 * server_and.c
 *
 * Receives bitwise AND operation jobs from the edge server, completes the
 * jobs, and sends the results to the edge server.
 *
 * Usage: ./server_and
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

#include <stdbool.h>

#define RECV_BYTES 29 // number of bytes received from edge server
#define SEND_BYTES 14 // number of bytes sent to edge server

#define OPERAND_BYTES 10 // maximum number of bytes used by operand
#define RESULT_BYTES 10 // maximum number of bytes used by result

#define AND_IP "127.0.0.1" // AND server IPv4 address
#define AND_PORT 22926 // AND server datagram socket port number

#define EDGE_IP "127.0.0.1" // edge server IPv4 address
#define EDGE_PORT 24926 // edge server datagram socket port number

/**
 * struct to store AND job data
 */
struct and_job {
   int job_number; 
   char operand1[OPERAND_BYTES + 1];
   char operand2[OPERAND_BYTES + 1];
   char result[RESULT_BYTES + 1];
};

/**
 * setupsocket creates a datagram socket and binds it.
 * @return int socket descriptor, -1 if unsuccessful
 */
int setupsocket();

/**
 * recvandjob receives an AND job from the edge server.
 * @param sock_desc int datagram socket descriptor
 * @param edge_addr_ptr pointer to edge server socket address
 * @param edge_addr_len socklen_t length of socket address
 * @param and_job_ptr pointer to and_job
 * @return int number of AND jobs, -1 if unsuccessful
 */
int recvandjob(int sock_desc, struct sockaddr_in * edge_addr_ptr,
   socklen_t edge_addr_len, struct and_job * and_job_ptr);

/**
 * andcalculation performs the bitwise AND calculation for an and_job array.
 * @param and_jobs and_job array
 * @param num_and_jobs int number of AND jobs
 * @return int 0 if successful, 1 if unsuccessful
 */
int andcalculation(struct and_job and_jobs[], int num_and_jobs);

/**
 * sendresults sends the results to the edge server.
 * @param sock_desc int datagram socket descriptor
 * @param edge_addr_ptr pointer to edge server datagram socket address
 * @param and_jobs and_job array
 * @param num_jobs int number of jobs
 * @param num_and_jobs int number of AND jobs
 * @return int 0 if successful, 1 if unsuccessful
 */
int sendresults(int sock_desc, struct sockaddr_in * edge_addr_ptr,
   struct and_job and_jobs[], int num_and_jobs);

/**
 * main
 * socket is setup, jobs are received from the edge server, bitwise AND
 * operation jobs are completed, and the results are sent back to the edge
 * server.
 * @param argc int number of arguments
 * @param argv pointer to array of c string argument pointers
 * @return int 0 if successful, 1 if unsuccessful
 */
int main(void)
{
   // Setup datagram socket
   int sock_desc;
   if ((sock_desc = setupsocket()) == -1)
   {
      return EXIT_FAILURE;
   }

   // Specify edge server address information
   struct sockaddr_in edge_addr;
   edge_addr.sin_family = AF_INET;
   edge_addr.sin_port = htons(EDGE_PORT); // store in network byte order
   edge_addr.sin_addr.s_addr = inet_addr(EDGE_IP);
   memset(edge_addr.sin_zero, '\0', sizeof(edge_addr.sin_zero)); // allows for
      //typecasting

   socklen_t edge_addr_len = sizeof(edge_addr);

   while (1)
   {
      // Receive initial job from edge server to get number of AND jobs
      struct and_job and_job0;
      int num_and_jobs;
      if ((num_and_jobs = recvandjob(sock_desc, &edge_addr, edge_addr_len,
         &and_job0)) == -1)
      {
         continue;
      }

      // Print message indicating initial receipt of job(s) from edge server
      fprintf(stdout, "The AND server has started receiving jobs from the edge"
         " server for AND computation. The computation results are:\n");

      // Define array of structures to store job data
      struct and_job and_jobs[num_and_jobs];
      and_jobs[0] = and_job0;

      // Receive remaining jobs from edge server
      if (num_and_jobs > 1)
      {
         for (int i = 1; i < num_and_jobs; i++)
         {
            if (recvandjob(sock_desc, &edge_addr, edge_addr_len, &and_jobs[i])
               == EXIT_FAILURE)
            {
               continue;
            }
         }
      }

      // Perform bitwise AND operations
      andcalculation(and_jobs, num_and_jobs);

      // Send results to edge server
      if ((sendresults(sock_desc, &edge_addr, and_jobs, num_and_jobs))
         == EXIT_FAILURE)
      {
         continue;
      }
   }

   return EXIT_SUCCESS;
}

int setupsocket()
{
   // Create socket
   int sock_desc;

   if ((sock_desc = socket(PF_INET, SOCK_DGRAM, 0)) == -1)
   {
      fprintf(stderr, "ERROR: Failed to create socket.\n");
      return -1;
   }
   
   // Allow socket to reuse port
   int yes = 1;

   if (setsockopt(sock_desc, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))
      == -1)
   {
      fprintf(stderr, "ERROR: setsockopt for socket failed.\n");
      close(sock_desc);
      return -1;
   }
   
   // Specify socket address information
   struct sockaddr_in and_addr;
   and_addr.sin_family = AF_INET;
   and_addr.sin_port = htons(AND_PORT); // store in network byte order
   and_addr.sin_addr.s_addr = inet_addr(AND_IP);
   memset(and_addr.sin_zero, '\0', sizeof(and_addr.sin_zero)); // allows for
      //typecasting

   // Bind socket to address
   if (bind(sock_desc, (struct sockaddr *) &and_addr, sizeof(and_addr)) == -1)
   {
      fprintf(stderr, "ERROR: Failed to bind socket.\n");
      close(sock_desc);
      return -1;
   }
   
   // Print message indicating AND server is up and running
   fprintf(stdout, "The AND server is up and running using UDP on port %d.\n",
      AND_PORT);

   return sock_desc;
}

int recvandjob(int sock_desc, struct sockaddr_in * edge_addr_ptr,
   socklen_t edge_addr_len, struct and_job * and_job_ptr)
{
   char buffer[RECV_BYTES + 1];

   if (recvfrom(sock_desc, buffer, RECV_BYTES, 0,
      (struct sockaddr *) edge_addr_ptr, &edge_addr_len) != RECV_BYTES)
   {
      fprintf(stderr, "ERROR: Failed to receive job from edge server.\n");
      return -1;
   }
   buffer[RECV_BYTES] = '\0'; // append null character to buffer

   int num_and_jobs;

   // Extract data from edge server message
   if (sscanf(buffer, "%s %s %d %d", and_job_ptr->operand1,
      and_job_ptr->operand2, &(and_job_ptr->job_number), &num_and_jobs) != 4)
   {
      fprintf(stderr, "ERROR: Failed to extract fields from job message.\n");
      return -1;
   }

   return num_and_jobs;
}

int andcalculation(struct and_job and_jobs[], int num_and_jobs)
{
   for (int i = 0; i < num_and_jobs; i++)
   {
      char result[RESULT_BYTES + 1] = "";
      char large_op[OPERAND_BYTES + 1];
      char small_op[OPERAND_BYTES + 1];
      bool msb_found = false;

      if (strlen(and_jobs[i].operand1) >= strlen(and_jobs[i].operand2))
      {
         strcpy(large_op, and_jobs[i].operand1);
         strcpy(small_op, and_jobs[i].operand2);
      }
      else
      {
         strcpy(large_op, and_jobs[i].operand2);
         strcpy(small_op, and_jobs[i].operand1);
      }   

      for (int j = strlen(large_op) - strlen(small_op); j < strlen(large_op);
         j++)
      {
         if ((large_op[j] == '1') && (small_op[j - (strlen(large_op) -
            strlen(small_op))] == '1'))
         {
            strcat(result, "1");
            msb_found = true;
         }
         else if (msb_found)
         {
            strcat(result, "0");
         }
      }

      if (!msb_found)
      {
         strcat(result, "0");
      }

      strcpy(and_jobs[i].result, result);

      // Print message displaying AND computation result
      fprintf(stdout, "%s and %s = %s\n", and_jobs[i].operand1,
         and_jobs[i].operand2, result);
   }
   
   // Print message indicating all jobs were received and computations are
      //complete
   fprintf(stdout, "The AND server has successfully received %d jobs from the"
      " edge server and finished all AND computations.\n", num_and_jobs);

   return EXIT_SUCCESS;
}

int sendresults(int sock_desc, struct sockaddr_in * edge_addr_ptr,
   struct and_job and_jobs[], int num_and_jobs)
{
   for (int i = 0; i < num_and_jobs; i++)
      {
         char payload[SEND_BYTES + 1];

         sprintf(payload, "%3d %10s", and_jobs[i].job_number,
            and_jobs[i].result);

         if (sendto(sock_desc, payload, strlen(payload), 0,
            (struct sockaddr *) edge_addr_ptr, sizeof(*edge_addr_ptr))
            != SEND_BYTES)
         {
            fprintf(stderr, "ERROR: Failed to send result to edge server.\n");
            return EXIT_FAILURE;
         }
      }

   // Print message indicating all results have been sent to edge server
   fprintf(stdout, "The AND server has successfully finished sending all"
      " computation results to the edge server.\n");

   return EXIT_SUCCESS;
}

