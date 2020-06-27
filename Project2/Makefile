defualt:
	gcc -Wextra -Wall -g3 server.c -o server
	gcc -Wextra -Wall -g3 client.c -o client

clean:
	rm -rf server client *.file debug* *.tar.gz

dist:
	tar -czvf 205140928.tar.gz server.c client.c Makefile README packet.h
