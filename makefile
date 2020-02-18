all: server client

server: prog2_server.c trie.h
	gcc -o server prog2_server.c trie.c

client: prog2_client.c
	gcc -o client prog2_client.c

clean:
	rm -f client 
	rm -f server
