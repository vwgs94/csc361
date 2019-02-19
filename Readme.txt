Name: Victoria Sahle
Student Number: XXXXXXXXX
Lab: B07

Program: rdps.c, rdpr.c, send_rec_fun.c, additives.c

Program Description: CSC361 RDP protocol, implemented using the datagram (DGRAM) socket Application Programming Interface (API) with UDP. A text file will be transferred 
from a sender to a receiver through the Linksys router which emulates network impairments including packet loss, duplication, reordering and corruption in a real network.

Code Structure/Final design in RDP flow and error control: I have designed header data to be held within a struct (summary message) – this includes sequence number, acknowledgement number, window size,
payload length data of payload etc. I have implemented an initial 2 way (SYN, ACK) handshake between the sender and the receiver (using simple if else).
Once this is complete the sender will try to send the data accordingly to the receiver. To close the connection the sender will send a FIN packet, and 
the receiver will send FIN back (should actually send ACK back).The initial sequence number is just 0. Flow control is implemented by using the 
sliding window mechanism, with the receiver sending back a window to the sender. The size of this window, will then tell the sender how much data to send.
This mechanism can let the sender know, to slow down any data transmission if need be. The size of the window is dynamic (1024 bytes). I read and write
the file using using fopen, fread, fwrite, and fclose respectively. I am using a 'timeout_queue' for timeouts. Whenever the sender sends DATs, the return 
is assigned to this. I am also using a get_timedout_packet function which the timeout_queue is passed to. The function will return NULL if no packet has timeout.


*** A little test (88 KB) and a big test (approx. 6.3 MB) 
   have been provided in my submission ***

Other content provided:
A Makefile has been provided, with the following content:
all: clean rdp

rdp:
	gcc rdps.c send_rec_fun.c -o rdps -lreadline -lpthread -w
	gcc rdpr.c send_rec_fun.c -o rdpr -lreadline -lpthread -w

clean:
	rm -rf rdpr rdps

