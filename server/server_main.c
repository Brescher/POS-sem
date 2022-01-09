#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

int player_count = 0;
pthread_mutex_t mutexcount;

void write_client_int(int cli_sockfd, int msg)
{
    int n = write(cli_sockfd, &msg, sizeof(int));
    if (n < 0){
        perror("ERROR writing int to client socket");
        pthread_exit(NULL);
    }
}
void write_client_msg(int cli_sockfd, char * msg)
{
    int n = write(cli_sockfd, msg, strlen(msg));
    if (n < 0){
        perror("ERROR writing msg to client socket");
        pthread_exit(NULL);
    }
}

void write_clients_msg(int * cli_sockfd, char * msg)
{
    write_client_msg(cli_sockfd[0], msg);
    write_client_msg(cli_sockfd[1], msg);
}

void write_clients_int(int * cli_sockfd, int msg)
{
    write_client_int(cli_sockfd[0], msg);
    write_client_int(cli_sockfd[1], msg);
}

int recv_int(int cli_sockfd)
{
    int msg = 0;
    int n = read(cli_sockfd, &msg, sizeof(int));

    if (n < 0 || n != sizeof(int)) {
        return -1;
    }

    //prijatie správy

    return msg;
}


void get_clients(int lis_sockfd, int * cli_sockfd)
{
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;

    //cakanie na klientov

    int num_conn = 0;
    while(num_conn < 2)
    {
        listen(lis_sockfd, 2);

        memset(&cli_addr, 0, sizeof(cli_addr));

        clilen = sizeof(cli_addr);

        cli_sockfd[num_conn] = accept(lis_sockfd, (struct sockaddr *) &cli_addr, &clilen);

        if (cli_sockfd[num_conn] < 0) {
            perror("ERROR accepting a connection from a client.");
            pthread_exit(NULL);
        }

        //prijatie klienta

        write(cli_sockfd[num_conn], &num_conn, sizeof(int));

        //napisanie klientovi jeho id

        pthread_mutex_lock(&mutexcount);
        player_count++;
        printf("Počet hráčov je: %d. socket %d\n", player_count, cli_sockfd[num_conn]);
        pthread_mutex_unlock(&mutexcount);

        if (num_conn == 0) {
            write_client_msg(cli_sockfd[0],"HLD");
            //klient 1 caka na klienta 2
        }
        num_conn++;
    }
}

int get_player_move(int cli_sockfd)
{
    write_client_msg(cli_sockfd, "TRN");

    return recv_int(cli_sockfd);
}

int check_move(char board[][3], int move, int player_id)
{
    if ((move == 9) || (board[move/3][move%3] == ' ')) {
        return 1;
    }
    else {
        return 0;
    }
}

void update_board(char board[][3], int move, int player_id)
{
    board[move/3][move%3] = player_id ? 'X' : 'O';
}

void draw_board(char board[][3])
{
    printf(" %c | %c | %c \n", board[0][0], board[0][1], board[0][2]);
    printf("-----------\n");
    printf(" %c | %c | %c \n", board[1][0], board[1][1], board[1][2]);
    printf("-----------\n");
    printf(" %c | %c | %c \n", board[2][0], board[2][1], board[2][2]);
}

void send_update(int * cli_sockfd, int move, int player_id)
{
    write_clients_msg(cli_sockfd, "UPD");

    write_clients_int(cli_sockfd, player_id);

    write_clients_int(cli_sockfd, move);
}

int check_board(char board[][3], int last_move)
{
    int row = last_move/3;
    int col = last_move%3;

    if ( board[row][0] == board[row][1] && board[row][1] == board[row][2] ) {
        return 1;
    }
    else if ( board[0][col] == board[1][col] && board[1][col] == board[2][col] ) {
        return 1;
    }
    else if (!(last_move % 2)) {
        if ( (last_move == 0 || last_move == 4 || last_move == 8) && (board[1][1] == board[0][0] && board[1][1] == board[2][2]) ) {
            return 1;
        }
        if ( (last_move == 2 || last_move == 4 || last_move == 6) && (board[1][1] == board[0][2] && board[1][1] == board[2][0]) ) {
            return 1;
        }
    }
    return 0;
}

void *run_game(void *thread_data)
{
    int *cli_sockfd = (int*)thread_data;
    char board[3][3] = { {' ', ' ', ' '},
                         {' ', ' ', ' '},
                         {' ', ' ', ' '} };

    printf("Hra začína!\n");

    write_clients_msg(cli_sockfd, "SRT");

    draw_board(board);

    int prev_player_turn = 1;
    int player_turn = 0;
    int game_over = 0;
    int turn_count = 0;
    while(!game_over) {
        if (prev_player_turn != player_turn) {
            write_client_msg(cli_sockfd[(player_turn + 1) % 2], "WAT");
        }

        int valid = 0;
        int move = 0;
        while(valid != 1) {
            move = get_player_move(cli_sockfd[player_turn]);
            if (move == -1) break;
            printf("Hráč %d dal symbol na pozíciu: %d\n", player_turn, move);

            valid = check_move(board, move, player_turn);
            if (valid == 0) {
                printf("Neplatný ťah. Skús znova...\n");
                write_client_msg(cli_sockfd[player_turn], "INV");
            }
        }

        if (move == -1) {
            printf("Hráč sa odpojil.\n");
            break;
        } else if (move == 9) {
            printf("Hráč %d sa odpojil.\n", player_turn);
            write_client_msg(cli_sockfd[(player_turn + 1) % 2], "DC");
            write_client_msg(cli_sockfd[(player_turn)], "QUI");
            break;
        } else {
            update_board(board, move, player_turn);
            send_update( cli_sockfd, move, player_turn );

            draw_board(board);

            game_over = check_board(board, move);

            if (game_over == 1) {
                write_client_msg(cli_sockfd[player_turn], "WIN");
                write_client_msg(cli_sockfd[(player_turn + 1) % 2], "LSE");
                printf("Hráč %d vyhral.\n", player_turn);
            }
            else if (turn_count == 8) {
                printf("Remíza.\n");
                write_clients_msg(cli_sockfd, "DRW");
                game_over = 1;
            }

            prev_player_turn = player_turn;
            player_turn = (player_turn + 1) % 2;
            turn_count++;

            //printf("Hráč %d , hrac2 %d, a scoket %d.\n", cli_sockfd[0], cli_sockfd[1], *cli_sockfd);
        }
    }

    printf("Koniec hry.\n");

    close(cli_sockfd[0]);
    close(cli_sockfd[1]);

    pthread_mutex_lock(&mutexcount);
    player_count--;
    printf("Počet hráčov je: %d.\n", player_count);
    player_count--;
    printf("Počet hráčov je: %d.\n", player_count);
    pthread_mutex_unlock(&mutexcount);

    free(cli_sockfd);

    pthread_exit(NULL);
}


int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr,"ERROR, no port provided\n");
        exit(1);
    }

    //vytovrenie socketu na pocuvanie
    int sockfd;
    struct sockaddr_in serv_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("ERROR creating listener socket.");
        pthread_exit(NULL);
    }

    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(atoi(argv[1]));

    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR binding listener socket.");
        pthread_exit(NULL);
    }

    int lis_sockfd = sockfd;
    pthread_mutex_init(&mutexcount, NULL);



    //cakanie na klientov
    if (player_count < 2) {
        int *cli_sockfd = (int*)malloc(2*sizeof(int));
        bzero(cli_sockfd, 2*sizeof(int));

        get_clients(lis_sockfd, cli_sockfd);

        //zacina nova hra, vytvara sa vlakno
        pthread_t thread;
        int result = pthread_create(&thread, NULL, run_game, (void *)cli_sockfd);
        if (result){
            printf("Thread creation failed with return code %d\n", result);
            exit(-1);
        }
        pthread_join(thread, NULL);
    }

    close(sockfd);
    close(lis_sockfd);

    pthread_mutex_destroy(&mutexcount);

    pthread_exit(NULL);

}
