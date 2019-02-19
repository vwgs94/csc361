/***********************************************************************
    Name: Victoria Sahle
    Student Number: XXXXXXXXX
    Program: shared.c
    Program Description: Shared functions for both rdps.c and rdpr.c
************************************************************************/
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
char* _type_[] = { "DAT", 
		   "ACK", 
		   "SYN", 
		   "FIN", 
	           "RST" };
/*idea from https://github.com/Hoverbear/CSC-361/tree/master/Assignment2redo*/
void chartingPacket(char event_type, struct sockaddr_in* source, struct sockaddr_in* destination, packet_t* the_packet) {
  
  char time_string[100];
  struct timeval tv;
  time_t nowtime;
  struct tm *nowtm;

  gettimeofday(&tv, NULL);

  nowtime = tv.tv_sec;
  nowtm = localtime(&nowtime);
  strftime(time_string, 100, "%H:%M:%S", nowtm);

  fprintf(stdout, "%s.%06li %c %s:%d ", time_string, (long int) tv.tv_usec,
          event_type, inet_ntoa(source->sin_addr), source->sin_port);
  fprintf(stdout, "%s:%d %s %d/%d %d/%d\n", inet_ntoa(destination->sin_addr),
          destination->sin_port, _type_[the_packet->type], the_packet->seqno,
          the_packet->ackno, the_packet->payload, the_packet->window);

  return;
}

/* print details to user */
void chartingPacketCount(summary_message* packetCount, int is_sender) {
  struct timeval now;
  gettimeofday(&now, NULL);
  struct timeval difference;
  timersub(&now, &packetCount->start_time, &difference);
  if (is_sender) {
    fprintf(stdout, "total data bytes send: %d\n unique data bytes sent: %d\n total data packets sent: %d\n unique data packets sent: %d\n SYN packets sent: %d\n FIN packets sent: %d\n RST packets send: %d\n ACK packets recieved: %d\n RST packets recieved: %d\n total time duration (second): %d.%d\n",
                     packetCount->total_data, packetCount->unique_data, packetCount->total_packets, packetCount->unique_packets, packetCount->SYN, packetCount->FIN, packetCount->RST, packetCount->ACK, packetCount->RST_2, (int) difference.tv_sec, (int) difference.tv_usec);
  } else {
    fprintf(stdout, "total data bytes recieved: %d\n unique data bytes reieved: %d\n total data packets recieved: %d\n unique data packets recieved: %d\n SYN packets recieved: %d\n FIN packets recieved: %d\n RST packets recieved: %d\n ACK packets sent: %d\n RST packet sent: %d\n total time duration (second): %d.%d\n",
                     packetCount->total_data, packetCount->unique_data, packetCount->total_packets, packetCount->unique_packets, packetCount->SYN, packetCount->FIN, packetCount->RST, packetCount->ACK, packetCount->RST_2, (int) difference.tv_sec, (int) difference.tv_usec);
  }
  return;
}


/* http://www.cse.yorku.ca/~oz/hash.html djb2 hash */
unsigned long hFun(char *str) {
  unsigned long hFun = 5381;
  int c;
  while ((c = *str++)) {
    hFun = ((hFun << 5) + hFun) + c; /* hFun * 33 + c */
  }
  return hFun;
}

packet_t* packetProcessing(char* source) {
  packet_t* result = calloc(1, sizeof(struct packet_t));
  char* Ws;
  /* check the checksum. */

  unsigned long checksum = strtoul(source, &Ws, 10);
 
  Ws++;

  if (checksum != hFun(Ws)) {
    return NULL;
  }

  /* checking the magic */
  if (strncmp(Ws, "CSc361", 6) != 0) {
    return NULL;
  }
  Ws += 7; 

  /* check the type */
  int i;
  for (i=0; i<5; i++) {
    if (strncmp(Ws, _type_[i], 3) == 0) {
      result->type = i;
      break;
    }
  }
  Ws += 3;

  /* get the seqno */
  Ws++;
  result->seqno = (int) strtoul(Ws, &Ws, 10);
  
  /* get the ackno */
  Ws++;
  result->ackno = (int) strtoul(Ws, &Ws, 10);
  
  /* get the payload */
  Ws++;
  result->payload = (int) strtoul(Ws, &Ws, 10);
  
  Ws++;
  result->window = (int) strtoul(Ws, &Ws, 10);
  
  /* get the data */
  Ws++;
  result->data = calloc(MAX_PAYLOAD_LENGTH, sizeof(char));
  result->next = NULL;
  sprintf(result->data, "%s", Ws);
  return result;
}

char* getReadyPacket(packet_t* source) {
  char* pre_checksum = calloc(MAX_PACKET_LENGTH+1, sizeof(char));
  
  sprintf(pre_checksum, "CSc361 %s %d %d %d %d\n%s", _type_[source->type],
          source->seqno, source->ackno, source->payload, source->window, source->data);
  char* result = calloc(MAX_PACKET_LENGTH+1, sizeof(char));
  sprintf(result, "%lu\n%s", hFun(pre_checksum), pre_checksum);
  
  return result;
}

/* we will set initial seqno */
int synPacketSend(int socket_fd, struct sockaddr_in* peer_address, socklen_t peer_address_size, struct sockaddr_in* host_address) {
  int seqno = 0;
  
  /* steps to build a SYN packet */
  packet_t syn_packet;
  syn_packet.type = SYN;
  syn_packet.seqno = seqno;
  syn_packet.ackno = 0;
  syn_packet.payload  = 0;
  syn_packet.window = 0;
  syn_packet.data = calloc(1, sizeof(char));
  strcpy(syn_packet.data, "");
  char* syn_string = getReadyPacket(&syn_packet);
  /* all done send our syn packet */
  sendto(socket_fd, syn_string, MAX_PACKET_LENGTH, 0, (struct sockaddr*) peer_address, peer_address_size);
  chartingPacket('s', host_address, peer_address, &syn_packet);

  free(syn_packet.data);
  free(syn_string);

  return seqno;
}

/* timed out packet from the queue */
packet_t* findTimeout(packet_t* timeout_queue) {
  packet_t* head = timeout_queue;
  struct timeval now;
  gettimeofday(&now, NULL);
 
  while (head != NULL) {
    if (!timerisset(&head->timeout) || timercmp(&now, &head->timeout, >)) {
      return head;
    }
    head = head->next;
  }
  return NULL;
}

/* we will try to send enough DAT packets to fill up the window */
packet_t* dataSendToWindow(int socket_fd, struct sockaddr_in* host_address, struct sockaddr_in* peer_address, socklen_t peer_address_size, FILE* file, int* current_seqno, packet_t* last_ack, packet_t* timeout_queue, enum interaction* detail_interaction) {
  packet_t* head = timeout_queue;

  /* calculate the number of packets to send given the window size */
  int initial_seqno = *current_seqno;

  /* this should give us the number of packets we want to send */
  int packets_to_send = MAX_WINDOW_SIZE_IN_PACKETS - ((initial_seqno - last_ack->ackno)  / MAX_PAYLOAD_LENGTH);
  int sent_packets = 0;

  while (sent_packets < packets_to_send) {
    /* here read in data from file */
    int seqno_increment = 0;
    packet_t* packet = calloc(1, sizeof(struct packet_t));
    packet->data = calloc(MAX_PAYLOAD_LENGTH+1, sizeof(char));
    if ((seqno_increment = (int) fread(packet->data, sizeof(char), MAX_PAYLOAD_LENGTH, file)) == 0) {
        /* build */
        packet->type     = FIN;
        packet->seqno    = *current_seqno;
        packet->ackno    = 0;
        packet->payload  = 0;
        packet->window   = 0;
        strcpy(packet->data, "");
        timerclear(&packet->timeout);
        /* send */
        head = time_it(head, packet);
        *detail_interaction = ext;
      break;
    } else {
      /* build */
      packet->type     = DAT;
      packet->seqno    = *current_seqno; // TODO: Might need +1
      packet->ackno    = 0;
      packet->payload  = seqno_increment;
      packet->window   = 0;
      timerclear(&packet->timeout);
       /* send */
      head = time_it(head, packet);
      /* increment num packets sent */
      if (seqno_increment != MAX_PAYLOAD_LENGTH) {
        sent_packets++; // Don't the seqno if this is the FIN.
      } else {
        sent_packets++;
        *current_seqno += seqno_increment;
      }
    }
  }
  /* in case altered */
  return head;
}

/* send an ACK for SYN */
void acknowPacketSend(int socket_fd, struct sockaddr_in* host_address, struct sockaddr_in* peer_address, socklen_t peer_address_size, int seqno, int window_size) {
  /* build ACK packet */
  packet_t ack_packet;
  ack_packet.type     = ACK;
  ack_packet.seqno    = 0;
  ack_packet.ackno    = seqno;
  ack_packet.payload  = 0;
  ack_packet.window   = window_size;
  ack_packet.data     = calloc(1, sizeof(char));
  strcpy(ack_packet.data, "");
  char* ack_string    = getReadyPacket(&ack_packet);
  chartingPacket('s', host_address, peer_address, &ack_packet);
  /* send our ack packet */
  sendto(socket_fd, ack_string, MAX_PACKET_LENGTH, 0, (struct sockaddr*) peer_address, peer_address_size);

  free(ack_packet.data);
  free(ack_string);

  return;
}

/* send an RST for the given seqno */
void resetPacketSend(int socket_fd, struct sockaddr_in* host_address, struct sockaddr_in* peer_address, socklen_t peer_address_size, int seqno, int window_size) {
  /* build an ACK pkt */
  packet_t ack_packet;
  ack_packet.type     = RST;
  ack_packet.seqno    = 0;
  ack_packet.ackno    = seqno;
  ack_packet.payload  = 0;
  ack_packet.window   = window_size;
  ack_packet.data     = calloc(1, sizeof(char));
  strcpy(ack_packet.data, "");
  char* ack_string    = getReadyPacket(&ack_packet);
  chartingPacket('s', host_address, peer_address, &ack_packet);
  /* send it */
  sendto(socket_fd, ack_string, MAX_PACKET_LENGTH, 0, (struct sockaddr*) peer_address, peer_address_size);
  
  free(ack_packet.data);
  free(ack_string);
  
  return;
}

/* here resend a DAT pkt */
void resendPacketPlease(int socket_fd, struct sockaddr_in* peer_address, socklen_t peer_address_size, packet_t* packet, summary_message* packetCount) {
  
  struct timeval now;
  struct timeval post_timeout;
  /* packet count */
  packetCount->total_data += packet->payload;
  if (!timerisset(&packet->timeout)) {
    packetCount->unique_data += packet->payload;
  }
  if (packet->type == DAT) {
    packetCount->total_packets++;
    if (!timerisset(&packet->timeout)) {
      packetCount->unique_packets++;
    }
  }
  
  gettimeofday(&now, NULL);
  post_timeout.tv_sec = 1;
  timeradd(&now, &post_timeout, &packet->timeout);
  /* attempt to resend packet */
  char* buffer = getReadyPacket(packet);
  sendto(socket_fd, buffer, MAX_PACKET_LENGTH, 0, (struct sockaddr*) peer_address, peer_address_size);
  
  return;
}

packet_t* time_it(packet_t* timeout_queue, packet_t* packet) {
  packet_t* current_position = timeout_queue;
  if (current_position != NULL) {
    while (current_position->next != NULL) {
      current_position = current_position->next;
    }
    packet->next = current_position->next;
    current_position->next = packet;
    packet->timeout.tv_usec = 0;
  } else {
    timeout_queue = packet;
    packet->timeout.tv_usec = 0;
  }
  return timeout_queue;
}

/* remove packets up to the given pkts ack number */
packet_t* removePacket(packet_t* packet, packet_t* timeout_queue) {

  packet_t* head = timeout_queue;
  packet_t* current = head;
  /* find corresponding packet */
  if (current->seqno == packet->ackno) {
    head = current->next;
  } else {
    while (current != NULL && current->next != NULL) {
      if (current->next->seqno == packet->ackno) {
        /* dequeue */
        packet_t* target = current->next;
        current->next = target->next;
        free(target->data);
        free(target);
        break;
      } else {
        /* do nothing */
      }
      current = current->next;
    }
  }
    return head;
}

/* here we can write the file to buff */
packet_t* slidingWindow(packet_t* packet, packet_t* head, FILE* file, int* window_size) { 
  packet_t* selected_window_packet = head;
  int travelled = 0;
  int is_contiguous = 1;
  int ttl = MAX_WINDOW_SIZE_IN_PACKETS;
  /* walk through the packet list and find where this should go */
  if (selected_window_packet == NULL) {
    /* at the head of the window */
    head = packet; 
    /* return address of the new head */
    selected_window_packet = head;
    travelled++;
  } else {

    while (selected_window_packet->next != NULL && (ttl--) != 0) {
      /* find if there is 'rollover'*/
      int next_possible_seqno = selected_window_packet->seqno + MAX_PAYLOAD_LENGTH;
      if (selected_window_packet->next->seqno != next_possible_seqno) {
        /* we're not contiguous & can't flush window */
        is_contiguous = 0;
      }
      if (selected_window_packet->next->seqno == packet->seqno + MAX_PAYLOAD_LENGTH) {
        break;
      }
      selected_window_packet = selected_window_packet->next; 
      travelled++;
    }
    /* not at the head - we should insert after current pkt */
    packet->next = selected_window_packet->next;
    selected_window_packet->next = packet;
    selected_window_packet = selected_window_packet->next;
    travelled += 2;
  }
  
  if (is_contiguous && selected_window_packet != NULL && (travelled == MAX_WINDOW_SIZE_IN_PACKETS || selected_window_packet->payload < MAX_PAYLOAD_LENGTH)) {
   /* until this point should be flushed to file */
    while (selected_window_packet->next != NULL && !(selected_window_packet->next->seqno >= selected_window_packet->seqno + MAX_PAYLOAD_LENGTH)) {
      /* traverse rest of the nodes until we find a node which is not contiguous */
      selected_window_packet = selected_window_packet->next;
      travelled++;
    }
    while (head == selected_window_packet || head->seqno != selected_window_packet->seqno) {
      /* flush until here - loop through the pkts */
      fwrite(head->data, sizeof(char), head->payload, file);
      /* head of the window needs to change */
      head = head->next;  
      travelled--;
      if (head == NULL) { 
	break; 
      }
    }
    /* this is for window size */
    while (selected_window_packet->next != NULL) {
      travelled++;
      selected_window_packet = selected_window_packet->next;
    }
  }
  *window_size = MAX_WINDOW_SIZE_IN_PACKETS - travelled;
  
   return head;
}


