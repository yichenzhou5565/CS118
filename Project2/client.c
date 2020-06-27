#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <time.h>
#include <sys/types.h>
#include <stdbool.h>
#include <limits.h>
#include <fcntl.h>

#include "packet.h"

/* Global variables */
char* hostname;
unsigned short MYPORT;
char filename[20];
const int MAXSEQ = 25600;
const int MAXFILE = 10485760 ;           /* 10 MB = 10485760 byte */
bool handshake_over = false;
int exp_seq = 0;
int BEGIN_SEQ ;             /* sequence we begin with */
int sendFIN = 0;                        /* should we send fin or not */
int sent_data_num = 0;                  /* pkt we have already sent */
int FILE_SIZE = -1;                     /* size of file */
//int ATTEMPTED = -1;
int start_timer = 0;                    /* do we want timeout control */
int DONE = -1;                               /* pkts done i.e. rcv'ed ack */

/* Buffer for selective repeat */
char data[11][512];
char storage[50][512];
//char window[10][524];
struct PACKET window[10];

/* Helper functions */
void debug(struct PACKET* pkt);
int send_pkt(int sockfd, const void *buffer, size_t len, int flags, const struct sockaddr*dest_addr, socklen_t addr_len);
void debug_print_buffer(char buf[50][512]);
int all_pkt_success();

/* main routine */
int main (int argc, char* argv[])
{
    BEGIN_SEQ = rand() % (MAXSEQ+1);
    
    memset(data, '\0', sizeof(data));
    memset(window, 0, sizeof(window));
    //printf("%d\n", sizeof(window));
    /* Get info from input arguments */
    if (argc != 4)
    {
        fprintf(stderr, "Please input correct arguments: \n\t./client <HOSTNAME or IP> <PORT> <FILENAME>\n");
        exit(1);
    }
    char** endptr=NULL;
    hostname = argv[1];
    MYPORT = (unsigned short) strtoul(argv[2], endptr, 10);
    sprintf(filename, "%s", argv[3]);
    //printf("*%s* *%u* *%s*\n", hostname, MYPORT, argv[3]);
    
    /* Open the file to transfer */
    FILE* fp = fopen(filename, "rb");
    if (fp == NULL)
    {
        fprintf (stderr, "No such file exists.\n");
        exit(1);
    }
    
    fseek(fp, 0, SEEK_END);
    FILE_SIZE = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    //PKT_SENT_TIME = malloc( sizeof(long) * (FILE_SIZE / 512 + 1) );
    
    int byte_not_send = FILE_SIZE;
    
    /* Set up socket */
    int sockfd;
    struct sockaddr_in my_addr, srv_addr;
    struct hostent* hstnt;
    
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1)
    {
        fprintf(stderr, "Error in socket(). %s\n", strerror(errno));
        exit(1);
    }
    
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(MYPORT);
    my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    memset(my_addr.sin_zero, '\0', sizeof(my_addr.sin_zero));
    
    hstnt = gethostbyname(hostname);
    if (hstnt == NULL)
    {
        fprintf(stderr, "Error in gethostbyname.\n");
        exit(1);
    }
    
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(MYPORT);
    memset(&srv_addr.sin_zero, '\0', sizeof(srv_addr.sin_zero));
    memcpy(&srv_addr.sin_addr.s_addr, hstnt->h_addr_list[0], hstnt->h_length);
    socklen_t srv_addr_len = sizeof(srv_addr);
    
    /* 3-way handshake: send initial SYN packet
        to initiate data transfer */
    int prev_seq = 0;
    struct PACKET* syn_pkt;
    syn_pkt = add_pkt(0, 1, 0, 1, 0, BEGIN_SEQ);
    send_pkt(sockfd, syn_pkt, 12, 0, (struct sockaddr*)&srv_addr, sizeof(srv_addr));
    printf("SEND %d %d SYN\n", syn_pkt->seq_num, syn_pkt->ack_num);
    SYN_TIME = m_time();
    free(syn_pkt);
    SYN_TIME = m_time();
    //sleep(2);
    
    while(1)
    {
        char* buffer[525];
        struct PACKET* pkt_recv = malloc(sizeof(struct PACKET));
        int byte_recv = recvfrom(sockfd, buffer, 524, MSG_DONTWAIT, (struct sockaddr*)&srv_addr, &srv_addr_len);
        if (byte_recv< 0)
            continue;
        if (byte_recv == 0)
        {
            fprintf(stderr, "btye_recv < 0\n");
            if (SYN_TIME > 0 && SYN_TIME > m_time() - 500)
            {
                  printf("TIMEOUT %d\n", BEGIN_SEQ);
                  struct PACKET* pkt ;
                  pkt = add_pkt (0, 1, 0, 1, 0, BEGIN_SEQ);
                  printf("RESEND %d SYN\n",BEGIN_SEQ );
                  sendto(sockfd, pkt, 12, 0, (struct sockaddr*)&srv_addr, sizeof(srv_addr));
                  SYN_TIME = m_time();
                  free(pkt);
            }
            else if (FIN_cli_TIME > 0 && FIN_cli_TIME > m_time() - 500)
            {
                struct PACKET* fin_re;
                int add = (FILE_SIZE>512) ? (FILE_SIZE-512) : 0;
                int seq_here = 1 + BEGIN_SEQ + add;
                printf("TIMEOUT %d\n", seq_here);
                fin_re = add_pkt(0, 0, 1, 0, 0, seq_here);
                send_pkt(sockfd, fin_re, 12, 0, (struct sockaddr*)&srv_addr, sizeof(srv_addr));
                printf("RESEND %d FIN\n", fin_re->seq_num);
                FIN_cli_TIME = m_time();
                free(fin_re);
            }
            else
            {
                if (PKT_SENT_TIME[DONE + 1] > m_time() - 500)
                {
                    int total = (FILE_SIZE%521==0) ? FILE_SIZE/512 : FILE_SIZE/512+1;
                    int seq_here = (DONE+1!=total) ? (BEGIN_SEQ+1+(DONE+1)*512) : (BEGIN_SEQ+1+(DONE)*512+FILE_SIZE%512);
                    if (FILE_SIZE%512 == 0 && DONE+1 == total)
                        seq_here += 1;
                    printf("TIMEOUT %d\n", seq_here);
                    
                    /* RESEND all pkts in window */
                    int j;
                    for(j=0; j<10; j++)
                    {
                        printf("RESEND %d\n", window[j].seq_num);
                        send_pkt(sockfd, &window[j], 524, 0, (struct sockaddr*)&srv_addr, sizeof(srv_addr));
                        PKT_SENT_TIME[window[j].div_num] = m_time();
                    }
                }
            }
            continue;
        }
        memcpy(pkt_recv, buffer, byte_recv);
        
        printf("RECV %d %d ", pkt_recv->seq_num, pkt_recv->ack_num);
        if (pkt_recv->SYN == 1)
            printf("SYN ");
        if (pkt_recv->FIN == 1)
            printf("FIN ");
        if (pkt_recv->ACK == 1)
            printf("ACK");
        printf("\n");
        
        /* If receive syn-ack from server */
        if (pkt_recv->SYN == 1 && pkt_recv->ACK == 1 && sendFIN == 0)
        {
            SYNACK_TIME = -1;
            
            int new_seq = pkt_recv->ack_num;
            int new_ack = (pkt_recv->seq_num + 1) % (MAXSEQ + 1);
            struct PACKET* ack;
            
            /* read file to the data buffer */
            int i;
            for(i=0; i<10 ; i++)  /* win sz = 10 */
            {
                if (feof(fp))
                {
                    break;
                }
                fread(data[i], 1, 512, fp);
            }
            
            /* Send packets in pipeline */
            int j=0;
            int threeWayDone = 0;
            for (; j<i ; j++)
            {
                PKT_SENT_TIME[j] = m_time();
                if (threeWayDone == 0)
                {
                    ack = add_pkt(1, 0, 0, 1, new_ack, new_seq);
                    threeWayDone = 1;
                }
                else
                {
                    ack = add_pkt(0, 0, 0, 1, 0, new_seq);
                }
                
                int byte_sent;
                if (byte_not_send==0)
                {
                    //sendFIN += 1;
                    break;
                }
                if (byte_not_send >= 512)
                {
                    memcpy(ack->payload, data[j], 512);
                    byte_not_send -= 512;
                    ack->div_num = j;
                    byte_sent = send_pkt(sockfd, ack, 524, 0, (struct sockaddr*)&srv_addr, sizeof(srv_addr));
                }
                else
                {
                    memcpy(ack->payload, data[j], 512);
                    //printf("%d $$$$$$$\n", byte_not_send);
                    ack->div_num = j;
                    byte_sent = send_pkt(sockfd, ack, byte_not_send+12, 0, (struct sockaddr*)&srv_addr, sizeof(srv_addr));
                    byte_not_send = 0;
                }
                //ack->div_num = j;
                sent_data_num = j;
                
                memcpy(storage[ack->seq_num / 524], ack->payload, 512);
                memcpy(&window[j], ack, 524);
                
                printf("SEND %d %d ", ack->seq_num, ack->ack_num);
                if (ack->SYN == 1)
                    printf("SYN ");
                if (ack->ACK == 1)
                    printf("ACK ");
                printf("\n");
                
                new_seq += byte_sent - 12;
                new_seq %= MAXSEQ + 1;
                prev_seq = ack->seq_num;
                
                
            }
            
            if (feof(fp)!=0)
            {
                sendFIN += 1;
            }
            
            free(ack);
            
        }
        
        /* If receive fin message from server
            wait for 2 s and close connection*/
        else if (pkt_recv->FIN == 1)
        {
            /* send last ack message */
            struct PACKET* last_ack;
            int new_ack = (pkt_recv->seq_num+1) % (MAXSEQ + 1);
            int new_seq = (prev_seq + 1) % (MAXSEQ + 1);
            last_ack = add_pkt(1, 0, 0, 0, new_ack, new_seq);
            send_pkt(sockfd, last_ack, 12, 0, (struct sockaddr*)&srv_addr, sizeof(srv_addr));
            printf("SEND %d %d ACK\n", last_ack->seq_num, last_ack->ack_num);
            ACK_cli_TIME = m_time();
            
            /* wait for 2 seconds */
            sleep(2);
            
            /* close connection & free resources*/
            //free(PKT_SENT_TIME);
            free(last_ack);
            fclose(fp);
            
            FIN_cli_TIME = m_time();
            //printf("%d **************\n", DONE);
            
            exit(0);
            
            
        }
        
        /* If receive regular ACK message from server */
        //else if (sendFIN == 0)
        else if (! all_pkt_success())
        {
            start_timer = 1;
            int new_seq = pkt_recv->seq_num;
            int new_ack = prev_seq ;
            struct PACKET* data_pkt;
            data_pkt = add_pkt(0, 0, 0, 1, new_ack, new_seq);
            
            if (byte_not_send >= 512)
            {
                fread(data_pkt->payload, sizeof(char), 512, fp);
                data_pkt->div_num = sent_data_num + 1;
                PKT_SENT_TIME[data_pkt->div_num] = m_time();
                send_pkt(sockfd, data_pkt, 524, 0, (struct sockaddr*)&srv_addr, sizeof(srv_addr));
                byte_not_send -= 512;
            }
            else
            {
                fread(data_pkt->payload, sizeof(char), 512, fp);
                data_pkt->div_num = sent_data_num + 1;
                PKT_SENT_TIME[data_pkt->div_num] = m_time();
                send_pkt(sockfd, data_pkt, 12+byte_not_send, 0, (struct sockaddr*)&srv_addr, sizeof(srv_addr));
                byte_not_send = 0;
            }
            
            memcpy(storage[data_pkt->seq_num / 524], data_pkt->payload, 524);
            int j;
            for(j=0; j<10; j++)
            {
                struct PACKET* itr = &window[j];
                if (itr->div_num == pkt_recv->div_num)
                {
                    memcpy(&window[j], data_pkt, 524);
                }
            }
            
            sent_data_num += 1;
            if (pkt_recv->ACK == 1)
            {
                PKT_SENT_TIME[pkt_recv->div_num] = -1;
                if (DONE == pkt_recv->div_num - 1)
                {
                    DONE = pkt_recv->div_num;
                }
            }
            DONE = pkt_recv->div_num;
            //printf("%d *************", pkt_recv->div_num);
            printf("SEND %d %d\n", data_pkt->seq_num , data_pkt->ack_num);
            
            
            if ( feof(fp) != 0 )
            {
                sendFIN += 1;
            }
            prev_seq = data_pkt->seq_num;
            free(data_pkt);
        }
        
        
        /* check for timeout and retransmission */
        if (start_timer == 1)
        {
            int i = DONE + 1;
            //fprintf(stderr, "***** %ld\n ", PKT_SENT_TIME[i]);
            int tot = (FILE_SIZE%521==0) ? FILE_SIZE/512 : FILE_SIZE/512+1;
            while (i <= tot && PKT_SENT_TIME[i]!=0 && PKT_SENT_TIME[i] != -1)
            {
                /* timeout for i'th pkt*/
                if (m_time() > PKT_SENT_TIME[i] + 500)
                {
                    /* timeout */
                    int total = (FILE_SIZE%521==0) ? FILE_SIZE/512 : FILE_SIZE/512+1;
                    int seq_here = (i!=total) ? (BEGIN_SEQ+1+i*512) : (BEGIN_SEQ+1+(i-1)*512+FILE_SIZE%512);
                    if (FILE_SIZE%512 == 0 && i == total)
                        seq_here += 1;
                    printf("TIMEOUT %d\n", seq_here);
                    
                    /* RESEND all pkts in window */
                    int j;
                    for(j=0; j<10; j++)
                    {
                        printf("RESEND %d\n", window[j].seq_num);
//                        int total = FILE_SIZE / 512;
//                        if (FILE_SIZE % 512 != 0)
//                            total += 1;
                        if (window[j].div_num == total){
                            send_pkt(sockfd, &window[j], 12 + FILE_SIZE % 512, 0, (struct sockaddr*)&srv_addr, sizeof(srv_addr));
                            //printf("%d ################ ", FILE_SIZE % 512);
                            //i = INT_MAX;
                            //break;
                        }
                        else
                            send_pkt(sockfd, &window[j], 524, 0, (struct sockaddr*)&srv_addr, sizeof(srv_addr));
                        PKT_SENT_TIME[window[j].div_num] = m_time();
                    }
                    continue;
//                    /* retransmit pkt */
//                    struct PACKET* pkt_re;
//                    pkt_re = add_pkt(0, 0, 0, 1, 0, seq_here);
//                    memcpy(pkt_re->payload, storage[seq_here / 524], 512);
//                    if ((i+1) * 512 > FILE_SIZE)
//                        send_pkt(sockfd, pkt_re, 12+FILE_SIZE%512, 0, (struct sockaddr*)&srv_addr, sizeof(srv_addr));
//                    else
//                        send_pkt(sockfd, pkt_re, 524, 0, (struct sockaddr*)&srv_addr, sizeof(srv_addr));
//                    printf("RESEND %d\n", seq_here);
//                    free(pkt_re);
//                    break;
                }
                i++;
            }
        }
        
        /* timeout upon FIN */
        if (start_timer == 1 && FIN_cli_TIME != 0 && FIN_cli_TIME != -1)
        {
            if ( m_time() > FIN_cli_TIME + 500  )
            {
                struct PACKET* fin_re;
                int add = (FILE_SIZE>512) ? (FILE_SIZE-512) : 0;
                int seq_here = 1 + BEGIN_SEQ + add;
                printf("TIMEOUT %d\n", seq_here);
                fin_re = add_pkt(0, 0, 1, 0, 0, seq_here);
                send_pkt(sockfd, fin_re, 12, 0, (struct sockaddr*)&srv_addr, sizeof(srv_addr));
                printf("RESEND %d FIN\n", fin_re->seq_num);
                
            }
        }
        
        /* If we should send FIN now
         <=> start close down */
        //if (sendFIN == 1)
        if (all_pkt_success() == 1 && sendFIN >= 1)
        {
            struct PACKET* init_fin;
            int new_ack = 0;
            int new_seq = pkt_recv->ack_num;
            init_fin = add_pkt(0, 0, 1, 0, new_ack, new_seq);
            send_pkt(sockfd, init_fin, 12, 0, (struct sockaddr*)&srv_addr, sizeof(srv_addr));
            printf("SEND %d %d FIN\n", init_fin->seq_num, init_fin->ack_num);
            prev_seq = init_fin->seq_num;
            sendFIN += 1;
            free(init_fin);
            FIN_cli_TIME = m_time();
            sendFIN = 0;
        }
        
        
    }
    return(0);
}

/*****************************************************************/
/************************ END OF MAIN ****************************/
/*****************************************************************/

void debug(struct PACKET* pkt)
{
    fprintf(stderr, "**********\n");
    fprintf(stderr, "SEQ_no:%d ACK_no:%d ACK:%d SYN:%d FIN:%d FLG:%d\n", pkt->seq_num, pkt->ack_num, pkt->ACK, pkt->SYN, pkt->FIN, pkt->FLG);
    fprintf(stderr, "PAYLOAD:%s", pkt->payload);
    
}

int send_pkt(int sockfd, const void *buffer, size_t len, int flags,  const struct sockaddr* dest_addr, socklen_t addr_len)
{
    return sendto(sockfd, buffer, len, flags, dest_addr, addr_len);
}

void debug_print_buffer(char buf[50][512])
{
    int i=0;
    fprintf(stderr, "*****");
    for(; i<50 && buf[i]!='\0'; i++)
    {
        fprintf(stderr, "%s", buf[i]);
    }
    fprintf(stderr, "*****");
}

int all_pkt_success()
{
    int total = (FILE_SIZE%512 == 0) ? FILE_SIZE/512 : FILE_SIZE/512 + 1;
    if (total == DONE + 1)
        return 1;
    return 0;
}
