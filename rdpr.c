/**********************************************************************************************
    Name: Victoria Sahle
    Student Number: XXXXXXXXX
    Program: rdpr.c
    Program Description: CSC361 RDP protocol, implemented using the datagram (DGRAM) socket
    Application Programming Interface (API) with UDP. A text file will be transfered 
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
#include <sys/time.h>
#include "additives.h"

/* The packetCount for the session */
summary_message packetCount;

int main(int argc, char* argv[]) {
  if (argc != 4) {
    fprintf(stderr,"  rdpr receiver_ip receiver_port receiver_file_name");
    exit(-1);
  }

  /* holding arg data */
  char* receiver_ip = argv[1];
  int receiver_port = atoi(argv[2]);
  char* file_name = argv[3];
  
  /* open file/see exists */
  FILE* file = fopen(file_name, "w");
  
  /* setting up host */
  int sock  = socket(AF_INET, SOCK_DGRAM, 0);
  socklen_t receiver_address_size = sizeof(struct sockaddr_in);

  if (sock < 0) { 
	fprintf(stderr, "Couldn't create a socket."); 
	exit(-1); 
  }
 
  struct sockaddr_in   receiver_address;
  receiver_address.sin_family = AF_INET;
  receiver_address.sin_port = receiver_port;
  receiver_address.sin_addr.s_addr = inet_addr(receiver_ip);
  socklen_t sender_address_size = sizeof(struct sockaddr_in);
  struct sockaddr_in   sender_address;

  int socket_ops = 1;
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &socket_ops, sizeof(socket_ops)) < 0) {
    perror("Set socket failed.");
  }

  /* bind socket */
  if (bind(sock, (struct sockaddr*) &receiver_address, sizeof(receiver_address)) < 0) {
    perror("Bind to socket failed");
  }

  /* here I will set up packet count for later */
  gettimeofday(&packetCount.start_time, NULL);
  
  packet_t* file_head = NULL;
 
  int window_size = MAX_WINDOW_SIZE_IN_PACKETS;
  char* buffer = calloc(MAX_PACKET_LENGTH+1, sizeof(char));
  int acked_to = 0;

  char* packet_string;
  unsigned long first_addr = 0;
  for (;;) {
    /*Read from socket*/
    memset(buffer, '\0', MAX_PACKET_LENGTH);
    packet_t* packet;
    int bytes = recvfrom(sock, buffer, MAX_PACKET_LENGTH, 0, (struct sockaddr*) &sender_address, &sender_address_size); // This socket is blocking.
    if (bytes <= 0) {
      fprintf(stderr, "Error in receiving.\n");
    }
    /* must we reset ? */
    if (first_addr == 0) {
      first_addr = sender_address.sin_addr.s_addr;
    } else if (first_addr != sender_address.sin_addr.s_addr) {
      resetPacketSend(sock, &receiver_address, &sender_address, sender_address_size, packet->seqno, 0);
      exit(-1);
    }
    
    packet = packetProcessing(buffer);
    if (packet == NULL) {
      fprintf(stderr, "Packet corrupt, dropped");
      continue;
    }
    if (packet->seqno < acked_to) {
      packetCount.total_data += packet->payload;
      packetCount.total_packets++;
      acknowPacketSend(sock, &receiver_address, &sender_address, sender_address_size, packet->seqno, 0);
      continue;
    }
    char log_type = 'r';
    if (file_head != NULL && packet->seqno < file_head->seqno) {
      log_type = 'R';
    }
    chartingPacket(log_type, &receiver_address, &sender_address, packet);
    /* here we must check the packet type and act accordingly */
    /*idea from https://github.com/Hoverbear/CSC-361/tree/master/Assignment2redo*/
    switch (packet->type) {
      case SYN:
        /* Since we have received a SYN send back an ACK */
        packetCount.SYN++;
        packetCount.ACK++;
        acked_to = packet->seqno;
        acknowPacketSend(sock, &receiver_address, &sender_address, sender_address_size, packet->seqno, (int) (MAX_PAYLOAD_LENGTH * window_size));
        break;
      case DAT:
        packetCount.total_packets++;
        if (log_type == 'r') {
          packetCount.unique_packets++;
          
        }
        packetCount.total_data += packet->payload;
        if (log_type == 'r') {
          packetCount.unique_data += packet->payload;
        }
        /* here we write data to window*/
        /*that corresponding method will also update the seqno */
        file_head = slidingWindow(packet, file_head, file, &window_size); // THIS UPDATED WINDOW_SIZE
        packetCount.ACK++;
        acked_to = packet->seqno;
        acknowPacketSend(sock, &receiver_address, &sender_address, sender_address_size, packet->seqno, (int) (MAX_PAYLOAD_LENGTH * window_size));
        break;
      case RST:
        packetCount.RST++;
        acknowPacketSend(sock, &receiver_address, &sender_address, sender_address_size, packet->seqno, (int) (MAX_PAYLOAD_LENGTH * window_size));
        exit(-1);
        break;
      case FIN:
        /* a FIN can be sent back since we have completed the file :)*/
        packetCount.FIN++;
        packet_string = getReadyPacket(packet);
        /* here we should be sending*/
        chartingPacket('s', &receiver_address, &sender_address, packet);
        sendto(sock, packet_string, MAX_PACKET_LENGTH, 0, (struct sockaddr*) &sender_address, sender_address_size);
        /* the queue should be delt with*/
        while (file_head != NULL) {
          fprintf(file, "%s", file_head->data);
          file_head = file_head->next;
        }
        chartingPacketCount(&packetCount, 0);
        exit(0);
        break;
    }
  }
  
  return 0;
}
