#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>


void recv_msg(int sockfd, char * msg)
{
    memset(msg, 0, 4);
    int n = read(sockfd, msg, 3);

    if (n < 0) {
        perror("CHYBA pri čítaní správy zo servera.");
    }
}

int recv_int(int sockfd)
{
    int msg = 0;
    int n = read(sockfd, &msg, sizeof(int));

    if (n < 0) {
        perror("CHYBA pri čítaní čísla na server.");
    }
    return msg;
}

void write_server_int(int sockfd, int msg)
{
    int n = write(sockfd, &msg, sizeof(int));
    if (n < 0) {
        perror("CHYBA pri písaní čísla na server.");
    }
}

void error(const char *msg)
{
    perror(msg);
    printf("Buď server padol, alebo sa druhý hráč odpojil.\nKoniec hry.\n");

    exit(0);
}

void draw_board(char board[3][3]){
    printf(" %c | %c | %c \n", board[0][0], board[0][1], board[0][2]);
    printf("-----------\n");
    printf(" %c | %c | %c \n", board[1][0], board[1][1], board[1][2]);
    printf("-----------\n");
    printf(" %c | %c | %c \n", board[2][0], board[2][1], board[2][2]);
}

void take_turn(int sockfd)
{
    char buffer[10];

    while (1) {
        printf("Zadaj číslo 0-8 aby si spravil ťah, alebo 9 pre ukončenie hry: ");
	    fgets(buffer, 10, stdin);
	    int move = buffer[0] - '0';
        char msg = buffer[10];
        if (move <= 9 && move >= 0){
            printf("\n");
            write_server_int(sockfd, move);
            break;
        }
        else {
            printf("\nZlý input. Skús znova.\n");
        }
    }
}

void get_update(int sockfd, char board[][3])
{

    int player_id = recv_int(sockfd);
    int move = recv_int(sockfd);
    board[move/3][move%3] = player_id ? 'X' : 'O';
}


int main(int argc, char *argv[])
{
    int sockfd;
    struct sockaddr_in serv_addr;
    struct hostent* server;
    //pripajanie na server
    if (argc < 3) {
        fprintf(stderr,"usage %s hostname port\n", argv[0]);
        exit(0);
    }

    server = gethostbyname(argv[1]);
    if (server == NULL) {
        fprintf(stderr,"CHYBA, taký host neexistuje\n");
        exit(0);
    }

    memset(&serv_addr, 0, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    bcopy(
            (char*)server->h_addr,
            (char*)&serv_addr.sin_addr.s_addr,
            server->h_length
    );
    serv_addr.sin_port = htons(atoi(argv[2]));

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("CHYBA pri otváraní socketu pre server.");
        exit(0);
    }

    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0){
        perror("CHYBA pri pripájaní sa na server");
    }

    printf("Pripojený na server.\n");

    //start hry
    int id = recv_int(sockfd);

    char msg[4];
    char board[3][3] = { {' ', ' ', ' '},
                         {' ', ' ', ' '},
                         {' ', ' ', ' '} };

    printf("Piškvôrky\n---------\n");

    do {
        recv_msg(sockfd, msg);
        if (!strcmp(msg, "HLD")){
            printf("Čaká sa na druhého hráča...\n");
        }
    } while ( strcmp(msg, "SRT") );

    /* The game has begun. */
    printf("Hra začína!\n");
    printf("Ty si %c's\n", id ? 'X' : 'O');

    draw_board(board);

    while(1) {
        recv_msg(sockfd, msg);

        if (!strcmp(msg, "TRN")) {
	        printf("Si na ťahu...\n");
	        take_turn(sockfd);
        } else if (!strcmp(msg, "INV")) {
            printf("Tá pozícia je obsadená. Skús znova.\n");
        } else if (!strcmp(msg, "UPD")) {
            get_update(sockfd, board);
            draw_board(board);
        } else if (!strcmp(msg, "WAT")) {
            printf("Čaká sa na ťah druhého hráča...\n");
        } else if (!strcmp(msg, "WIN")) {
            printf("Vyhral si!\n");
            break;
        } else if (!strcmp(msg, "LSE")) {
            printf("Prehral si.\n");
            break;
        } else if (!strcmp(msg, "DRW")) {
            printf("Remíza.\n");
            break;
        } else if (!strcmp(msg, "DC")){
            printf("Protihráč sa odpojil.\n");
            break;
        } else if (!strcmp(msg, "QUI")){
            printf("Odpojili ste sa..\n");
            break;
        }
        else {
            error("Neznáma správa.");
        }
    }

    printf("Koniec hry.\n");
    close(sockfd);
    return 0;
}
