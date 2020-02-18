#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <sys/select.h>

void err(int sd);

int main( int argc, char **argv) {
	struct hostent *ptrh; /* pointer to a host table entry */
	struct protoent *ptrp; /* pointer to a protocol table entry */
	struct sockaddr_in sad; /* structure to hold an IP address */
	int sd; /* socket descriptor */
	int port; /* protocol port number */
	char *host; /* pointer to host name */
	int n; /* number of characters read */
	char buf[1000]; /* buffer for data from the server */

	memset((char *)&sad,0,sizeof(sad)); /* clear sockaddr structure */
	sad.sin_family = AF_INET; /* set family to Internet */

	if( argc != 3 ) {
		fprintf(stderr,"Error: Wrong number of arguments\n");
		fprintf(stderr,"usage:\n");
		fprintf(stderr,"./client server_address server_port\n");
		exit(EXIT_FAILURE);
	}

	port = atoi(argv[2]); /* convert to binary */
	if (port > 0) /* test for legal value */
		sad.sin_port = htons((u_short)port);
	else {
		fprintf(stderr,"Error: bad port number %s\n",argv[2]);
		exit(EXIT_FAILURE);
	}

	host = argv[1]; /* if host argument specified */

	/* Convert host name to equivalent IP address and copy to sad. */
	ptrh = gethostbyname(host);
	if ( ptrh == NULL ) {
		fprintf(stderr,"Error: Invalid host: %s\n", host);
		exit(EXIT_FAILURE);
	}

	memcpy(&sad.sin_addr, ptrh->h_addr, ptrh->h_length);

	/* Map TCP transport protocol name to protocol number. */
	if ( ((long int)(ptrp = getprotobyname("tcp"))) == 0) {
		fprintf(stderr, "Error: Cannot map \"tcp\" to protocol number");
		exit(EXIT_FAILURE);
	}

	/* Create a socket. */
	sd = socket(PF_INET, SOCK_STREAM, ptrp->p_proto);
	if (sd < 0) {
		fprintf(stderr, "Error: Socket creation failed\n");
		exit(EXIT_FAILURE);
	}

	/* Connect the socket to the specified server. */
	if (connect(sd, (struct sockaddr *)&sad, sizeof(sad)) < 0) {
		fprintf(stderr,"connect failed\n");
		exit(EXIT_FAILURE);
	}

  /////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////

	#define X 256

  char    status = 'd';
	int 		i;
	char 		turn;
	uint8_t round;
  char 		player;
	uint8_t result;
	uint8_t wordlen;
	uint8_t boardSize;
  uint8_t opp_score;
	uint8_t self_score;
	int 		no_mistake;
  uint8_t secPerRound;

  // get game information
  if(recv(sd, &player, sizeof(char), MSG_WAITALL) < 0){
    fprintf(stderr, "player info\n");
  }
  if(recv(sd, &boardSize, sizeof(uint8_t), MSG_WAITALL) < 0){
    fprintf(stderr, "board size\n");
  }
  if(recv(sd, &secPerRound, sizeof(uint8_t), MSG_WAITALL) < 0){
    fprintf(stderr, "sec per round\n");
  }

	char word[X];
	char board[boardSize];

  // print player information
  if(player == '1'){
    printf("You are Player 1... the game will begin when Player 2 joins...\n");
  }
  else{
    printf("You are Player 2...\n");
  }
  // print game information
  printf("Board size: %d\n", boardSize);
  printf("Seconds per turn: %d\n", secPerRound);

  // ROUND LOOP //
  while(1){

    // GET SCORE //
    if(recv(sd, &self_score, sizeof(uint8_t), MSG_WAITALL) < 0){
      err(sd);
    }
    if(recv(sd, &opp_score, sizeof(uint8_t), MSG_WAITALL) < 0){
      err(sd);
    }

		if(self_score == 3 || opp_score == 3){

		  if(self_score == 3){
		    printf("You won!\n");
		  }
		  else if(opp_score == 3){
		    printf("You lost!\n");
		  }

			close(sd);
			exit(EXIT_SUCCESS);
		}

    // GET ROUND NUM //
    if(recv(sd, &round, sizeof(uint8_t), MSG_WAITALL) < 0){
      err(sd);
    }
    printf("Round %d...\n", round);

    printf("Score is %d-%d\n", self_score, opp_score);

    // GET BOARD //
    if(recv(sd, &board, sizeof(board), MSG_WAITALL) <= 0){
      err(sd);
    }

    // PRINT BOARD
    printf("Board:");
    for(int i = 0; i < boardSize; i++){
      if(i == (boardSize-1)){
        printf(" %c\n", board[i]);
      }
      else{
        printf(" %c", board[i]);
      }
    }

		no_mistake = 1;
		// TURN LOOP // WHILE NO INVALID WORDS //
    while(no_mistake){

      // determine who's turn
      if(recv(sd, &turn, sizeof(char), MSG_WAITALL) <= 0){
        err(sd);
      }

			// MY TURN //
      if(turn == 'Y'){

        printf("Your turn, enter word: ");

        fflush(stdout);
				fd_set fd;
				int retval;

        // set for select
        FD_ZERO(&fd);
				FD_SET(0, &fd);
				FD_SET(sd, &fd);
				retval = select(sd + 1, &fd, NULL, NULL, NULL);

				// select error
        if(retval < 0){
					fprintf(stderr, "select\n");
					break;
				}

        if(FD_ISSET(sd, &fd)){
          // timeout
					if(recv(sd, &retval, sizeof(uint8_t), MSG_WAITALL) <= 0){
	        	err(sd);
	      	}
					printf("\nInvalid Word!\n");
					no_mistake = 0;
				}

        if(FD_ISSET(0, &fd)){

          fgets(word, X, stdin);
				  wordlen = strlen(word);

          // send len
  				if(write(sd, &wordlen, sizeof(uint8_t)) <= 0){
            err(sd);
          }

					// send word
          if(write(sd, &word, wordlen * sizeof(char)) <= 0){
            err(sd);
          }

          // CHECK WORD //
  				if(recv(sd, &result, sizeof(uint8_t), MSG_WAITALL) <= 0){
            err(sd);
          }

  				// valid word
          if(result == 1){
            printf("Valid word!\n");
          }
          // invalid word
          else{
            printf("Invalid word!\n");
            no_mistake = 0;
          }
        }
      }

			// OPPONENTS TURN //
      else if(turn == 'N'){

				// wait for other player
        printf("Please wait for opponent to enter word...\n");

				// get size of word or 0 for invalid
        if(recv(sd, &result, sizeof(uint8_t), MSG_WAITALL) <= 0){
          err(sd);
        }

        // valid word
        if(result > 0){

					// get word
          if(recv(sd, &word, result, MSG_WAITALL) <= 0){
            err(sd);
          }
					// cut off newline
          word[result--] = '\0';
          word[result++] = '\0';
          printf("Opponent entered \"%s\" \n", word);
        }
        // invalid word
        else{
          printf("Opponent lost the round!\n");
					no_mistake = 0;
        }
      }
    }
  }
}

// send error
// recv or send problem
void err(int sd){
  shutdown(sd, SHUT_RDWR);
  _exit(EXIT_FAILURE);
}
