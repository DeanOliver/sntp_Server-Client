/* talker.c - a datagram 'client'
 * need to supply host name/IP and one word message,
 * e.g. talker localhost hello */

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
#include <stdint.h>
#include <sys/time.h>
#include <math.h>
#define PORT 1123     /* server port the client connects to */

struct packet
{
	uint8_t flags;
	uint8_t stratum;
	uint8_t poll;
	uint8_t precision;
	uint32_t root_delay;
	uint32_t root_dispersion;
	char reference_identifier[4];
	uint32_t reference_timestamp;
	uint32_t reference_timestamp_fraction;
	uint32_t originate_timestamp;
	uint32_t originate_timestamp_fraction;
	uint32_t receieve_timestamp;
	uint32_t receieve_timestamp_fraction;
	uint32_t transmit_timestamp;
        uint32_t transmit_timestamp_fraction;
};

int main( int argc, char * argv[]) 
{
	int sockfd, numbytes, structlength;
	int quit = 0;
	uint32_t delay, offset;         // In seconds
	uint32_t fdelay, foffset;       // In Micro Seconds
	struct hostent *he;
	struct sockaddr_in their_addr;    // My address to bind to receive datagram, server address info
	struct packet request_packet;     // Packet sent to server
	struct packet response_packet;    // Packet recieved from server in network byte order
	struct packet complete_packet;    // The Complete packet in host byte order
	struct timeval tv, t_out,arrival_tv; // Storing the time of day
	int i;

	if(argv[1] == NULL)
	{
		printf("Usage: ./sntpClientUpdate <ip address OR hostname>");
		exit(1);
	}

	// resolve server host name or IP address
	if( (he = gethostbyname( argv[ 1])) == NULL) 
	{
		perror( "Failed to resolve address");
		exit( 1);
	}

	if( (sockfd = socket( AF_INET, SOCK_DGRAM, 0)) == -1) 
	{
		perror( "Client socket");
		exit( 1);
	}

	t_out.tv_sec = 3;               // Set how long the client is willing to wait,
	t_out.tv_usec = 100000;         // for a packet to be sent back from a server

	if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &t_out,sizeof(t_out)) < 0)
	{
		perror ("Client setsockopt");
		exit(1);
	}

	memset( &their_addr,0, sizeof(their_addr));  // zero struct 
	their_addr.sin_family = AF_INET;     // host byte order .. 
	their_addr.sin_port = htons( PORT);  // .. short, netwk byte order 
	their_addr.sin_addr = *((struct in_addr *)he -> h_addr);

	memset( &request_packet,0, sizeof(request_packet));  // zero the struct  
	request_packet.flags = 0x23;
	gettimeofday(&tv, NULL); // set the struct tv with the curret time of day in UNIX time.
	// Convert sec and usec in network byte order 
	request_packet.transmit_timestamp = htonl(tv.tv_sec + 2208988800); 
	request_packet.transmit_timestamp_fraction = htonl((tv.tv_usec * 4294967925)/1000000);

	structlength = sizeof(their_addr);

	start: // The restart point for sending pakets

	if( (numbytes = sendto( sockfd, (void *)&request_packet, sizeof(request_packet), 0, (struct sockaddr *)&their_addr, sizeof(their_addr))) == -1) 
	{
		perror( "Client sendto");
		exit( 1);
	}

	printf( "Sent %d bytes to %s\n", numbytes, inet_ntoa( their_addr.sin_addr));

	// Waiting the set amount of time to recieve a response
	if( (numbytes = recvfrom( sockfd, (void *)&response_packet, sizeof(response_packet), 0, (struct sockaddr *)&their_addr, (socklen_t*) &structlength )) == -1) 
	{
		perror( "Client recvfrom");
		exit(1);
	}

	gettimeofday(&arrival_tv, NULL); //Time packet arrived T4

	switch (numbytes)        // Checks the packet is a correct size
	{
		case 48:break;
		case 52:break;
		case 64:break;
		case 68:break;
		default: printf("Incorrect packet size"); exit(1); break;
        }

	arrival_tv.tv_usec = ((arrival_tv.tv_usec * 4294967925)/1000000);

	printf( "Got packet from %s\n", inet_ntoa( their_addr.sin_addr));
	printf( "Packet is %d bytes long\n", numbytes);
       
	if(response_packet.flags & 0x04)   // Checks the  
	{
		if (response_packet.stratum == 0)
		{
			printf("Kiss-o-death received \n");
			exit(1);
		}

		//Convert and store all the recieved data
		complete_packet.reference_timestamp = ntohl(response_packet.reference_timestamp) - 2208988800; // Atomic CLock
		complete_packet.reference_timestamp_fraction = ntohl(response_packet.reference_timestamp_fraction);
		complete_packet.originate_timestamp = ntohl(response_packet.originate_timestamp) - 2208988800; // My original time T1
		complete_packet.originate_timestamp_fraction = ntohl(response_packet.originate_timestamp_fraction);
		complete_packet.receieve_timestamp = ntohl(response_packet.receieve_timestamp) - 2208988800;   // When my packet was recieved T2
		complete_packet.receieve_timestamp_fraction = ntohl(response_packet.receieve_timestamp_fraction);
		complete_packet.transmit_timestamp = ntohl(response_packet.transmit_timestamp) - 2208988800;   // Time server sent packet T3
		complete_packet.transmit_timestamp_fraction = ntohl(response_packet.transmit_timestamp_fraction);
 
		// Dealing with the time in seconds
		// delay = (T4 - T1) - (T3 - T2)
		delay = (arrival_tv.tv_sec - complete_packet.originate_timestamp) - (complete_packet.transmit_timestamp - complete_packet.receieve_timestamp);

		// System CLock Offset = ((T2 - T1) + (T3 - T4))/2
		offset = ((complete_packet.receieve_timestamp - complete_packet.originate_timestamp) + (complete_packet.transmit_timestamp - arrival_tv.tv_sec)) / 2;
         
		printf("Delay in seconds is:  %u\n", delay);
		printf("Offset in seconds is:  %u\n\n", offset);

		uint32_t time = complete_packet.reference_timestamp + offset;
		printf("Time in seconds is %u\n", time);

		// Dealing with the time in fractions of seconds (Micro Seconds)
		// delay = (T4 - T1) - (T3 - T2) - Total delay of sending
		fdelay = (arrival_tv.tv_usec - complete_packet.originate_timestamp_fraction) - (complete_packet.transmit_timestamp_fraction - complete_packet.receieve_timestamp_fraction);

		// System CLock Offset = ((T2 - T1) + (T3 - T4))/2 - average time of each send
		foffset = ((complete_packet.receieve_timestamp_fraction - complete_packet.originate_timestamp_fraction) + (complete_packet.transmit_timestamp_fraction - arrival_tv.tv_usec)) / 2;

		printf("fdelay is %u\n", fdelay);
		printf("foffset is %u\n", foffset);

		uint32_t ftime = complete_packet.reference_timestamp_fraction + foffset;
		printf("ftime is %u\n", ftime);
		printf("Unix time in micro seconds : %u.%u\n", time, ftime);
	}
	else
	{
		printf("Not an NTP Packet\n");
		quit++;           
		
		if(quit == 5)     // If the packet sent is not an ntp packet
		{               // close the client after 5 attempts 
			exit(1);
		}
	
		goto start;      // Resend the packet.
	}
     
	close( sockfd);

	return 0;
}
