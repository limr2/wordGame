#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "trie.h"
#include <stdbool.h>
#include <ctype.h>

#define QLEN 6 /* size of request queue */
int visits = 0; /* counts client connections */

void rungame(int p1, int p2, int boardSize, int secPerRound, struct trie *trie);
int check_valid(struct trie *trie, struct trie *words_used, char *word, char *board);
void err(int p1, int p2);

int main(int argc, char **argv) {
	struct protoent *ptrp; /* pointer to a protocol table entry */
	struct sockaddr_in sad; /* structure to hold server's address */
	struct sockaddr_in cad; /* structure to hold client's address */
	int sd ; /* socket descriptors */
	int p1, p2;
	int port; /* protocol port number */
	int alen; /* length of address */
	int optval = 1; /* boolean value when we set socket option */
	char buf[1000]; /* buffer for string the server sends */

	if( argc != 5 ) {
		fprintf(stderr,"Error: Wrong number of arguments\n");
		fprintf(stderr,"usage:\n");
		fprintf(stderr,"./server server_port\n");
		exit(EXIT_FAILURE);
	}

	memset((char *)&sad,0,sizeof(sad)); /* clear sockaddr structure */
	sad.sin_family = AF_INET; /* set family to Internet */
	sad.sin_addr.s_addr = INADDR_ANY; /* set the local IP address */

	port = atoi(argv[1]); /* convert argument to binary */
	if (port > 0) { /* test for illegal value */
		sad.sin_port = htons((u_short)port);
	} else { /* print error message and exit */
		fprintf(stderr,"Error: Bad port number %s\n",argv[1]);
		exit(EXIT_FAILURE);
	}

	/* Map TCP transport protocol name to protocol number */
	if ( ((long int)(ptrp = getprotobyname("tcp"))) == 0) {
		fprintf(stderr, "Error: Cannot map \"tcp\" to protocol number");
		exit(EXIT_FAILURE);
	}

	/* Create a socket */
	sd = socket(PF_INET, SOCK_STREAM, ptrp->p_proto);
	if (sd < 0) {
		fprintf(stderr, "Error: Socket creation failed\n");
		exit(EXIT_FAILURE);
	}

	/* Allow reuse of port - avoid "Bind failed" issues */
	if( setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0 ) {
		fprintf(stderr, "Error Setting socket option failed\n");
		exit(EXIT_FAILURE);
	}

	/* Bind a local address to the socket */
	if (bind(sd, (struct sockaddr *)&sad, sizeof(sad)) < 0) {
		fprintf(stderr,"Error: Bind failed\n");
		exit(EXIT_FAILURE);
	}

	/* Specify size of request queue */
	if (listen(sd, QLEN) < 0) {
		fprintf(stderr,"Error: Listen failed\n");
		exit(EXIT_FAILURE);
	}

	// args converted
	int boardSize = atoi(argv[2]);
  int secPerRound = atoi(argv[3]);
	// sendable format
	uint8_t bs = boardSize;
	uint8_t spr = secPerRound;

	// read tw106.txt and put in array dict
  #define  X 256
	FILE*		 fp;
	char		 buffer[X];
	char*		 filename = argv[4];

	struct trie* trie = trie_new();
  fp = fopen(filename, "r");

  if (fp == NULL){
    printf("couldn't open file\n");
    return 1;
  }
  while (fgets(buffer, X, fp) != NULL){
		trie_add_word(trie, buffer, strlen(buffer), 0);
  }
  fclose(fp);

	// always look for connections
	while (1) {
		alen = sizeof(cad);
		// connect with player 1
		if ( (p1=accept(sd, (struct sockaddr *)&cad, &alen)) < 0) {
			fprintf(stderr, "Error: Accept failed\n");
			exit(EXIT_FAILURE);
		}

		// player 1 information send
		char i = '1';
		send(p1, &i, sizeof(char), 0);
		send(p1, &bs, sizeof(uint8_t), 0);
		send(p1, &spr, sizeof(uint8_t), 0);

		// connect with player 2
		if ( (p2=accept(sd, (struct sockaddr *)&cad, &alen)) < 0) {
			fprintf(stderr, "Error: Accept failed\n");
			exit(EXIT_FAILURE);
		}

		// player 2 information send
		i = '2';
		send(p2, &i, sizeof(char), 0);
		send(p2, &bs, sizeof(uint8_t), 0);
		send(p2, &spr, sizeof(uint8_t), 0);

		// create instance
		if(fork() == 0){
			rungame(p1, p2, boardSize, secPerRound, trie);
      trie_free(trie);
			exit(1);
		}
	}
}

void rungame(int p1, int p2, int boardSize, int secPerRound, struct trie *trie){

	int 		turn; // store what turn it is
  int     check; // valid word or not
	uint8_t round; // store what round it is
	uint8_t wordlen; // leghth of word recieved
	int			no_mistake; // no invalid words yet
	char 		word[boardSize]; // store word recieved
	char 		board[boardSize]; // current round's game board

  struct trie *words_used = trie_new();

	char no = 'N';
	char yes = 'Y';
	uint8_t invalid = 0;
	uint8_t valid = 1;

  struct timeval tv;
	tv.tv_sec = secPerRound;
	tv.tv_usec = 0;

	round = 1;
	uint8_t p1Score = 0;
	uint8_t p2Score = 0;

	// ROUND LOOP //
	while(1){

    // reset words_used //
    trie_free(words_used);
    words_used = trie_new();

		// SEND SCORE //
		// self
		if(send(p1, &p1Score, sizeof(uint8_t), 0) < 0){
  		err(p1, p2);
  	}
		if(send(p2, &p2Score, sizeof(uint8_t), 0) < 0){
  		err(p1, p2);
  	}
		// opp
		if(send(p1, &p2Score, sizeof(uint8_t), 0) < 0){
  		err(p1, p2);
  	}
		if(send(p2, &p1Score, sizeof(uint8_t), 0) < 0){
  		err(p1, p2);
  	}

    // SEND ROUND //
		if(send(p1, &round, sizeof(uint8_t), 0) < 0){
  		err(p1, p2);
  	}
		if(send(p2, &round, sizeof(uint8_t), 0) < 0){
  		err(p1, p2);
  	}

		// MAKE NEW BOARD //
		int i = 0;
		int vowel = 0;
		while(i < boardSize){
			board[i] = "abcdefghijklmnopqrstuvwxyz"[random () % 26];
			// is vowel
			if(strchr("aeiou", board[i]) != NULL){
				vowel = 1;
			}
      // last iteration no vowel, keep going
			if(i == (boardSize - 1)){
				if(vowel){
					i++;
				}
			}
			else{
				i++;
			}
		}
		board[i] = '\0';

		// SEND BOARD //
		if(send(p1, &board, sizeof(board), 0) < 0){
  		err(p1, p2);
  	}
		if(send(p2, &board, sizeof(board), 0) < 0){
  		err(p1, p2);
  	}

		// DETERMINE TURN //
		if((round % 2) == 0){
			// even round, player 2
			turn = 2;
		}
		else{
			// odd round, player 1
			turn = 1;
		}

		no_mistake = 1;
		// TURN LOOP // WHILE NO INVALID WORDS //
		while(no_mistake){

			// even round
			if((turn % 2) == 0){
        // send N to player 1
				if(send(p1, &no, sizeof(char), 0) < 0){
      		err(p1, p2);
      	}
        // send Y to player 2
				if(send(p2, &yes, sizeof(char), 0) < 0){
      		err(p1, p2);
      	}

        // player 2 select initialize
        fd_set read;
        tv.tv_sec = secPerRound;
        FD_ZERO(&read);
  			FD_SET(p2, &read);

        // select
        select(p2 + 1, &read, NULL, NULL, &tv);
        // check if recieved
  			if(FD_ISSET(p2, &read)){
          // recieve length
  				if(recv(p2, &wordlen, sizeof(uint8_t), MSG_WAITALL) <= 0){
  					err(p1, p2);
  				}
          // recieve word
  				if(recv(p2, &word, wordlen * sizeof(char), MSG_WAITALL) <= 0){
  					err(p1, p2);
  				}
          // set end of string
          word[wordlen] = '\0';
          // check if word is valid
          check = check_valid(trie, words_used, word, board);
        }
        // timeout
        else{
          // invalid word
          check = 0;
			  }
			}
			// odd round
			else{
        // send Y to player 1
				if(send(p1, &yes, sizeof(char), 0) < 0){
      		err(p1, p2);
      	}
        // send N to player 2
				if(send(p2, &no, sizeof(char), 0) < 0){
      		err(p1, p2);
      	}

        // player 1 select initialize
        fd_set read;
        tv.tv_sec = secPerRound;
        FD_ZERO(&read);
  			FD_SET(p1, &read);

        // select
        select(p1 + 1, &read, NULL, NULL, &tv);
        // check if recieved
  			if(FD_ISSET(p1, &read)){
          // recieve length
  				if(recv(p1, &wordlen, sizeof(uint8_t), MSG_WAITALL) <= 0){
  					err(p1, p2);
  				}
          // recieve word
  				if(recv(p1, &word, wordlen * sizeof(char), MSG_WAITALL) <= 0){
  					err(p1, p2);
  				}
          // set end of string
          word[wordlen] = '\0';
          // check if word is valid;
          check = check_valid(trie, words_used, word, board);
        }
        // time out
        else{
          // invalid word
          check = 0;
			  }
      }

			// valid word
			if(check){
				// player 2
				if((turn % 2) == 0){
          // send valid to curr player
					if(send(p2, &valid, sizeof(uint8_t), 0) < 0){
            err(p1, p2);
          }
          // send wordlen to other player
					if(send(p1, &wordlen, sizeof(uint8_t), 0) < 0){
            err(p1, p2);
          }
          // send word to other player
					if(send(p1, &word, wordlen * sizeof(char), 0) < 0){
            err(p1, p2);
          }
				}
				// player 1
				else{
          // send valid to curr player
          if(send(p1, &valid, sizeof(uint8_t), 0) < 0){
            err(p1, p2);
          }
          // send wordlen to other player
					if(send(p2, &wordlen, sizeof(uint8_t), 0) < 0){
            err(p1, p2);
          }
          // send word to other player
					if(send(p2, &word, wordlen * sizeof(char), 0) < 0){
            err(p1, p2);
          }
				}
			}
			// invalid word
			else{
        // send invalid
				if(send(p1, &invalid, sizeof(uint8_t), 0) < 0){
          err(p1, p2);
        }
        // send invalid
				if(send(p2, &invalid, sizeof(uint8_t), 0) < 0){
          err(p1, p2);
        }

        // move onto next round
				no_mistake = 0;

				// player 1 gets point
				if((turn % 2 == 0)){
					p1Score++;
				}
				// player 2 gets point
				else{
					p2Score++;
				}
			}
      // next turn
			turn++;
		}

		// game isn't over yet
		if(p1Score != 3 && p2Score != 3){
			round++;
		}
		else{
			// send score one last time
			// self
			if(send(p1, &p1Score, sizeof(uint8_t), 0) < 0){
	  		err(p1, p2);
	  	}
			if(send(p2, &p2Score, sizeof(uint8_t), 0) < 0){
	  		err(p1, p2);
	  	}
			// opp
			if(send(p1, &p2Score, sizeof(uint8_t), 0) < 0){
	  		err(p1, p2);
	  	}
			if(send(p2, &p1Score, sizeof(uint8_t), 0) < 0){
	  		err(p1, p2);
	  	}

			close(p1);
			close(p2);
		}
	}
}

// checks if word is valid or invalid
// returns 0 if invalid
int check_valid(struct trie *trie, struct trie *words_used, char *word, char *board){

  int i = 0;
  int j = 0;
  // clean up word
  while(i < strlen(word)){
    if(isalpha(word[i])){
      word[j] = word[i];
      j++;
    }
    i++;
  }
  word[j] = '\0';

  // too long or too short
  if(strlen(word) > strlen(board) || strlen(word) == 0){
    return 0;
  }

  // not in trie
  if(!trie_search(trie, word, strlen(word))){
    return 0;
  }

  // in words used array
  if(trie_search(words_used, word, strlen(word)) == 0){
    return 0;
  }
  // not in array, so add
  else{
    trie_add_word(words_used, word, strlen(word), 0);
  }

  // copy onto temp array
  char temparray[strlen(board)];
  strcpy(temparray, board);

  i = 0;
  // go through all letters
  while(i < strlen(word)){
    // letter not in board
    if(strchr(temparray, word[i]) == NULL){
      return 0;
    }
    else{
      // delete instance of letter
      int index = (strchr(temparray, word[i])) - temparray;
      temparray[index] = 1;
    }
    i++;
  }

  // passed all tests
  return 1;
}

// send error
// recv or send problem
void err(int p1, int p2){
  shutdown(p1, SHUT_RDWR);
  shutdown(p2, SHUT_RDWR);
  _exit(EXIT_FAILURE);
}
