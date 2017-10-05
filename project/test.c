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

int main(void)
{
   int divisor = 128;
   char result_str[11] = "";
   bool msb_found = false;

   int result = 5;   

   while (result != 0)
   {  
      if (result / divisor == 1)
      {
         msb_found = true;

         strcat(result_str, "1");
         result = result % divisor;
      }
      else if (msb_found == true)
      {
         strcat(result_str, "0");
      }
      
      divisor = divisor / 2;
   }

   if (msb_found == false)
   {
      strcat(result_str, "0");
   }
   
   printf("%s\n", result_str);

   return 0;
}
