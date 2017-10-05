/**
 * client.c
 *
 * Reads an input file containing bitwise and/bitwise or operation jobs,
 * submits the jobs to an edge server, receives the results, and displays them.
 *
 * Usage: ./client <input_filename>
 *
 * The input file should list one job per line with the following format.
 * 
 * operator,operand1,operand2
 * 
 * operator must be "and" or "or"
 * operands must be in binary with a maximum of 10 digits
 *
 * Example: and,1010101,100
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define MAX_ROWS 100 // maximum number of rows allowed
#define MAX_ROW_BYTES 26 // maximum number of characters in row allowed

#define SEND_BYTES 29 // number of bytes sent to edge server
#define RECV_BYTES 10 // number of bytes received from edge server

#define EDGE_IP "127.0.0.1" // edge server IPv4 address
#define EDGE_PORT 23926 // edge server port number

typedef char jobsarr[MAX_ROWS][MAX_ROW_BYTES + 1]; // 2D array type for storing
   //jobs

/**
 * readjobs reads the input file and stores its contects in a 2D array.
 * @param filename pointer to char array containing name of input file
 * @param jobs_ptr pointer to jobsarr
 * @return int number of jobs read or -1 if unsuccessful
 */
int readjobs(char * filename, jobsarr * jobs_ptr);

/**
 * setupsocket creates a stream socket connected to the edge server.
 * @return int socket descriptor, -1 if unsuccessful
 */
int setupsocket();

/**
 * sendjobs sends all jobs to the edge server.
 * @param sock_desc int socket descriptor
 * @param jobs_ptr pointer to jobsarr
 * @param num_jobs int number of jobs
 * @return int 0 if successful, 1 if unsuccessful
 */
int sendjobs(int sock_desc, jobsarr * jobs_ptr, int num_jobs);

/**
 * recvresults receives results from the edge server and prints them on the
 * command line.
 * @param sock_desc int socket descriptor
 * @param num_jobs int number of jobs
 * @return int 0 if successful, 1 if unsuccessful
 */
int recvresults(int sock_desc, int num_jobs);

/**
 * main
 * input file is read, jobs are sent to edge server, results are received and
 * printed.
 * @param argc int number of arguments
 * @param argv pointer to array of c string argument pointers
 * @return int 0 if successful, 1 if unsuccessful
 */
int main(int argc, char * argv[])
{
   // Check command line arguments
	if (argc != 2)
   {
      fprintf(stderr, "ERROR: Usage: %s input_filename\n", argv[0]);
      return EXIT_FAILURE;
	}

   // Define 2 dimensional char array to store job strings
   char jobs[MAX_ROWS][MAX_ROW_BYTES + 1];
   jobsarr * jobs_ptr = &jobs;

   // Read input file and store job strings
   int num_jobs;
   if ((num_jobs = readjobs(argv[1], jobs_ptr)) == -1)
   {
      return EXIT_FAILURE;
   }

   // Setup stream socket to communicate with edge server
   int sock_desc;
   if ((sock_desc = setupsocket()) == -1)
   {
      return EXIT_FAILURE;
   }

   // Send jobs to edge server
   if (sendjobs(sock_desc, jobs_ptr, num_jobs) == EXIT_FAILURE)
   {
      return EXIT_FAILURE;
   }

   // Receive results from edge server
   if (recvresults(sock_desc, num_jobs) == EXIT_FAILURE)
   {
      return EXIT_FAILURE;
   }

   // Close socket
	close(sock_desc);

	return EXIT_SUCCESS;
}

int readjobs(char * filename, jobsarr * jobs_ptr)
{
   // Open input file
   FILE * file_ptr;

   if ((file_ptr = fopen(filename, "r")) == NULL)
   {
      fprintf(stderr, "ERROR: Unable to open %s\n", filename);
      return -1;
   }
   
   // Read jobs from input file and store
   int i = 0;
   
   while (fgets((*jobs_ptr)[i], MAX_ROW_BYTES + 1, file_ptr) != NULL)
   {
      (*jobs_ptr)[i][strlen((*jobs_ptr)[i]) - 1] = '\0'; // remove '\n'
         //character from end of string 
      
      if (strlen((*jobs_ptr)[i]) != 0)
      {
         for(int j = 0; j < strlen((*jobs_ptr)[i]); j++) // replace commas with
            //spaces
         {
            if ((*jobs_ptr)[i][j] == ',')
            {
               (*jobs_ptr)[i][j] = ' ';
            }
         }
         i++;
      }
   }
   int num_jobs = i;

   fclose(file_ptr);

   return num_jobs;
}

int setupsocket()
{
   // Create socket
   int sock_desc;

   if ((sock_desc = socket(PF_INET, SOCK_STREAM, 0)) == -1)
   {
      fprintf(stderr, "ERROR: Failed to create socket.\n");
      return -1;
   }

   // Specify edge server address information
   struct sockaddr_in edge_addr;
   edge_addr.sin_family = AF_INET;
   edge_addr.sin_port = htons(EDGE_PORT); // store in network byte order
   edge_addr.sin_addr.s_addr = inet_addr(EDGE_IP);
   memset(edge_addr.sin_zero, '\0', sizeof(edge_addr.sin_zero)); // allows for
      //typecasting

   // Connect to edge server
   if (connect(sock_desc, (struct sockaddr *) &edge_addr,
      sizeof(edge_addr)) == -1)
   {
      fprintf(stderr, "ERROR: Failed to connect socket.\n");
      close(sock_desc);
      return -1;
   }

   // Print message indicating client is up and running   
   fprintf(stdout, "The client is up and running.\n");

   return sock_desc;
}

int sendjobs(int sock_desc, jobsarr * jobs_ptr, int num_jobs)
{
   char payload[SEND_BYTES + 1];

   for (int j = 0; j < num_jobs; j++)
   {
      sprintf(payload, "%25s %3d", (*jobs_ptr)[j], num_jobs);

      if (send(sock_desc, payload, strlen(payload), 0) != SEND_BYTES)
      {
         fprintf(stderr, "ERROR: Failed to send job.\n");
         close(sock_desc);
         return EXIT_FAILURE;
      }
   }

   // Print message indicating client send jobs
   fprintf(stdout, "The client has successfully finished sending %d jobs to "
      "the edge server.\n", num_jobs);

   return EXIT_SUCCESS;
}

int recvresults(int sock_desc, int num_jobs)
{
   char results[num_jobs][RECV_BYTES + 1];

   for (int i = 0; i < num_jobs; i++)
   {   
      char buffer[RECV_BYTES + 1];  

      if (recv(sock_desc, &buffer, RECV_BYTES, 0) != RECV_BYTES)
      {
         fprintf(stderr, "ERROR: Failed to receive result.\n");
         close(sock_desc);
         return EXIT_FAILURE;
      }
	   buffer[RECV_BYTES] = '\0'; // append null character to buffer

      if (sscanf(buffer, "%s", results[i]) != 1)
      {
         fprintf(stderr, "ERROR: Failed to read result\n.");
         close(sock_desc);
         return EXIT_FAILURE;
      }
   }

   // Print message indicating all results are received
   fprintf(stdout, "The client has successfully finished receiving all "
      "computation results from the edge server.\nThe final computation results"
      " are:\n");

   for (int i = 0; i < num_jobs; i++)
   {
      fprintf(stdout, "%s\n", results[i]);
   }

   return EXIT_SUCCESS;
}

