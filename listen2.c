/* listener.c - a datagram socket 'server'
 * simply displays message received then dies!*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>         /* for gethostbyname() */
#include <sys/time.h>

#define MYPORT 1123        /* the port users connect to */

struct packet
{
	uint8_t flags;
	uint8_t stratum;
	uint8_t poll;
	uint8_t precision;
	uint32_t root_delay;
	uint32_t root_dispersion;
	uint32_t root_identifier;
	uint32_t reference_timestamp;
	uint32_t reference_timestamp_fraction;
	uint32_t originate_timestamp;
	uint32_t originate_timestamp_fraction;
	uint32_t receieve_timestamp;
	uint32_t receieve_timestamp_fraction;
	uint32_t transmit_timestamp;
        uint32_t transmit_timestamp_fraction;
};

int main( void) 
  {
     int sockfd;
     struct sockaddr_in my_addr;    /* info for my addr i.e. server */
     struct sockaddr_in their_addr; /* client's address info */
     struct packet request_packet, response_packet;
     struct timeval receive_tv, transmit_tv, reference_tv;
     socklen_t addr_len, numbytes;

     if( (sockfd = socket( AF_INET, SOCK_DGRAM, 0)) == -1) 
       {
          perror( "Listener socket");
          exit( 1);
       }

     memset( &my_addr, 0, sizeof( my_addr));    /* zero struct */
     my_addr.sin_family = AF_INET;              /* host byte order ... */
     my_addr.sin_port = htons(MYPORT); /* ... short, network byte order */
     my_addr.sin_addr.s_addr = INADDR_ANY;   /* any of server IP addrs */
     memset (&response_packet, 0, sizeof(response_packet));

     if( bind( sockfd, (struct sockaddr *)&my_addr, sizeof( struct sockaddr)) == -1) 
       {
          perror( "Listener bind");
          exit( 1);
       }

while(1)
  {
     addr_len = sizeof( struct sockaddr);

          if( (numbytes = recvfrom( sockfd, (void *)&request_packet, sizeof(request_packet), 0, (struct sockaddr *)&their_addr, &addr_len)) == -1) 
            {
               perror( "Listener recvfrom");
               exit( 1);
            }
        gettimeofday(&receive_tv, NULL);
     	printf( "Got packet from %s\n", inet_ntoa( their_addr.sin_addr));
     	printf( "Packet is %d bytes long\n", numbytes);	

	response_packet.flags = 0x24;
	response_packet.stratum = 1;
	response_packet.poll = request_packet.poll;
	gettimeofday(&reference_tv, NULL);
	response_packet.reference_timestamp = htonl(reference_tv.tv_sec + 2208988800);
	response_packet.reference_timestamp_fraction = htonl((reference_tv.tv_usec * 4294967925)/1000000);
	response_packet.originate_timestamp = request_packet.transmit_timestamp;
	response_packet.originate_timestamp_fraction = request_packet.transmit_timestamp_fraction;
	response_packet.receieve_timestamp = htonl(receive_tv.tv_sec + 2208988800);
	response_packet.receieve_timestamp_fraction = htonl((receive_tv.tv_usec * 4294967925)/1000000);
	
	gettimeofday(&transmit_tv, NULL);	
	response_packet.transmit_timestamp = htonl(transmit_tv.tv_sec + 2208988800);
        response_packet.transmit_timestamp_fraction = htonl((transmit_tv.tv_usec * 4294967925)/1000000);
	
     

          if( (numbytes = sendto( sockfd, (void *)&response_packet, sizeof(response_packet), 0, (struct sockaddr *)&their_addr, sizeof( struct sockaddr))) == -1) 
	  	{
			perror( "Talker sendto");
		  	exit( 1);
	        }

          printf( "Sent %d bytes to %s\n", numbytes, inet_ntoa( their_addr.sin_addr));
  }
     close( sockfd);
     return 0;
  }
