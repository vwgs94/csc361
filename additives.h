/*********************************
    Name: Victoria Sahle
    Student Number: XXXXXXXXX
    Program: shared.h
**********************************/

#include <stdlib.h>       
#include <stdio.h>      
#include <sys/types.h>

#define MAX_PACKET_LENGTH 1024
#define TIMEOUT 600             
#define MAX_PAYLOAD_LENGTH 900
#define MAX_WINDOW_SIZE_IN_PACKETS 10
#define MAX_SHORT 65535
#define MILLTONANO 1000000

/* Details of sender and receiver interaction */
enum interaction {
  handshake_connectionEst,
  data_trans,
  reset,
  ext
};

/* These are the types of packets */
enum packet_type {
  DAT,
  ACK,
  SYN,
  FIN,
  RST
};

/* This struct will hold important details of packets, including _type_, seqno, ackno etc. */

typedef struct packet_t {
  enum packet_type type;          
  int seqno;          
  int ackno;       
  int payload;       
  int window;                                           
  char* data;          
  struct timeval timeout;       
  struct packet_t* next;         
} packet_t;

/*This will hold the counts of the packets being transfered from sender to receiver, and vice-versa */

typedef struct summary_message {            
  int    total_data;                    
  int    unique_data;                    
  int    total_packets;                  
  int    unique_packets;                
  int    SYN;                            
  int    FIN;                            
  int    RST;                            
  int    ACK;                            
  int    RST_2;                          
  struct timeval start_time;             
} summary_message;

packet_t* time_it(packet_t* timeout_queue, packet_t* packet);
