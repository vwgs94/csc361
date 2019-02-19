/**********************************************************************************************
    Name: Victoria Sahle
    Student Number: XXXXXXXXX
    Program: rdps.c
    Program Description: CSC361 RDP protocol, implemented using the datagram (DGRAM) socket
    Application Programming Interface (API) with UDP. A text file will be data_transed 
    from a sender to a receiver through the Linksys router which emulates network impairments 
    including packet loss, duplication, reordering and corruption in a real network.
***********************************************************************************************/
#include <stdlib.h>       
#include <stdio.h>        
#include <sys/types.h>    
#include <string.h>       
#include <errno.h>
#include <assert.h>       
#include <unistd.h>       
#include <pthread.h>
#include <sys/socket.h>   
#include <netinet/in.h>   
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/time.h>
#include "additives.h"

/* The packetCount for the session */
summary_message packetCount;


int main(int argc, char* argv[]) {
  if (argc != 6) {
    fprintf(stderr,"Please follow the format: rdps sender_ip sender_port receiver_ip receiver_port sender_file_name");
    exit(-1);
  }

  /* holding arg data */
  char* sender_ip = argv[1];
  int sender_port = atoi(argv[2]);
  char* receiver_ip = argv[3];
  int receiver_port = atoi(argv[4]);
  char* file_name = argv[5];

  /* open file/see exists */
  FILE* file = fopen(file_name, "r");

  /* setting up host */
  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  struct sockaddr_in sender_address;
  socklen_t sender_address_size = sizeof(struct sockaddr_in);

  if (sock < 0) { 
	fprintf(stderr, "Couldn't create a socket."); 
	exit(-1); 
  }

  sender_address.sin_family = AF_INET;
  sender_address.sin_port = sender_port;
  sender_address.sin_addr.s_addr = inet_addr(sender_ip);

  /* setting up receiver */
  struct sockaddr_in receiver_address;
  receiver_address.sin_family = AF_INET;
  receiver_address.sin_port = receiver_port;
  receiver_address.sin_addr.s_addr = inet_addr(receiver_ip);
  socklen_t receiver_address_size = sizeof(struct sockaddr_in);

  int socket_ops = 1;
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&socket_ops, sizeof(socket_ops)) < 0) {
    perror("Set socket failed.");
  }

  /* bind socket */
  if (bind(sock, (struct sockaddr*) &sender_address, sizeof(sender_address)) < 0) {
    perror("Bind to socket failed");
  }

  fcntl(sock, F_SETFL, O_NONBLOCK);

  /* here I will set up packet count for later */
  gettimeofday(&packetCount.start_time, NULL);
 
  /* set the current detailed interaction to a handshake (2 way) */
  enum interaction detail_interaction = handshake_connectionEst;
  /* initial sequence number will be zero */
  int initial_seqno = 0;
  int system_seqno = initial_seqno;
  char* buffer = calloc(MAX_PACKET_LENGTH+1, sizeof(char));
  /* initialize the packet */
  packet_t* packet = NULL;
  /* this queue will be used for packet timeouts */
  packet_t* timeout_queue = NULL;
  
  while (detail_interaction == handshake_connectionEst) {
    int the_bytes = recvfrom(sock, buffer, MAX_PACKET_LENGTH+1, 0, (struct sockaddr*) &receiver_address, &receiver_address_size); // This socket is non-blocking.
    if (the_bytes <= 0) {
      packetCount.SYN++;
      initial_seqno = synPacketSend(sock, &receiver_address, receiver_address_size, &sender_address);
      nanosleep((struct timespec[]){{0, TIMEOUT * MILLTONANO}}, NULL);
    } else {
      detail_interaction = data_trans;
      packet = packetProcessing(buffer);     
      packetCount.ACK++;
      timeout_queue = dataSendToWindow(sock, &sender_address, &receiver_address, 
                              receiver_address_size, file, &system_seqno, 
                              packet, timeout_queue, &detail_interaction);
      free(packet);
    }
  }
  packet = NULL;
  /* will hold the type of event, we are currently dealing with*/
  char event_type; 

  for (;;) {
    while (packet == NULL) {
      /*Read from socket*/
      memset(buffer, '\0', MAX_PACKET_LENGTH+1);
      int bytes = recvfrom(sock, buffer, MAX_PACKET_LENGTH+1, 0, (struct sockaddr*) &receiver_address, &receiver_address_size);
      if (bytes == -1) {
        /* If there is a timed out packet go find it */
        packet = findTimeout(timeout_queue);
        if (packet != NULL && packet->timeout.tv_usec == 0) { 
          event_type = 's'; 
        } else {
          event_type = 'S'; 
        }
      } else {
        /* Parsing must occur since we have received our packet */
        packet = packetProcessing(buffer);
        event_type = 'r';
        
      }
    }
   if (event_type) {
      chartingPacket(event_type, &sender_address, &receiver_address, packet);
    }
    /* clean processing idea from https://github.com/Hoverbear/CSC-361/tree/master/Assignment2redo*/
    /* here we must check the packet type and act accordingly */
    switch (packet->type) {
      case ACK:
        packetCount.ACK++;
        /* Switch cases: figure out which stage we are in and do what is next */
        switch (detail_interaction) {
          case handshake_connectionEst:
            detail_interaction = data_trans;
            /* The connection has been established, and in other words we have completed the first handshake, and we are OK to send files */
            /* Until ACK received, sequence number will remain the same */
            system_seqno = 1;
            timeout_queue = dataSendToWindow(sock, &sender_address, &receiver_address, 
                              receiver_address_size, file, &system_seqno, 
                              packet, timeout_queue, &detail_interaction);
            break;
          case data_trans:
            /* Packet drop from timers */
            timeout_queue = removePacket(packet, timeout_queue);
              /* New DAT packets sent to fill window */
              timeout_queue = dataSendToWindow(sock, &sender_address, &receiver_address, 
                              receiver_address_size, file, &system_seqno, 
                              packet, timeout_queue, &detail_interaction);
            break;
          default:
            break;
        }
        break;
      case DAT:
        /* This packet we should resend */
        resendPacketPlease(sock, &receiver_address, receiver_address_size, packet, &packetCount);
        break;
      case RST:
        /* Connection must be restarted */
        detail_interaction = reset;
        exit(-1);
        break;
      case FIN:
        if (event_type == 'r') {
	  /* FIN packet received */
          chartingPacketCount(&packetCount, 1);
          exit(0);
        } else if (event_type == 'R') {
	  chartingPacketCount(&packetCount, 1);
          exit(0);
	}
	else {
          packetCount.FIN++;
          resendPacketPlease(sock, &receiver_address, receiver_address_size, packet, &packetCount);
          if (packetCount.FIN >= 3) {
            chartingPacketCount(&packetCount, 1);
            exit(0);
          }
        }
        break;
      default:
        fprintf(stderr, "I don't know what type packet this is.\n");
        break;
    }
    packet = NULL;
  }
  
  return 0;
}


