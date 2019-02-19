all: clean rdp

rdp:
	gcc rdps.c send_rec_fun.c -o rdps -lreadline -lpthread -w
	gcc rdpr.c send_rec_fun.c -o rdpr -lreadline -lpthread -w

clean:
	rm -rf rdpr rdps