#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <netdb.h>

#include "packet.h"

/* Global variables */
int MYPORT;
int BEGIN_SEQ ;
int connection_order = 0;
const int MAXSEQ = 25600;
int expected_seq =  -1;
int DONE_PKT = -1;
char recv_buffer[50][512];
int synack_acknum = -1;
int fin_acknum = -1;
int fin_seqnum = -1;

/* Helper functions */
void debug(struct PACKET* pkt);
void send_pkt(int sockfd, const void *buffer, size_t len, int flags,  const struct sockaddr* dest_addr, socklen_t addr_len);

int main (int argc, char* argv[])
{
    BEGIN_SEQ = rand() % (MAXSEQ + 1);
    
    int l;
    for(l=0; l<50; l++)
        memset(recv_buffer[l], -2, 512);
    
    struct sockaddr_in cli_addr, my_addr;
    int sockfd;
    unsigned int cli_addr_len = sizeof(cli_addr);
    //char filerecv[10485760];
    FILE* fp=NULL;
    
    /* Get input arguments */
    if (argc != 2)
    {
        fprintf(stderr, "Please input correct arguments:\n\t./server <port number>\n");
        exit(1);
    }
    MYPORT = atoi(argv[1]);
    
    /* Initialization */
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(MYPORT);
    my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    /* setup socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        fprintf(stderr, "Error in socket(). %s\n", strerror(errno));
        exit(1);
    }
    if (bind(sockfd, (struct sockaddr*)&my_addr, sizeof(my_addr)) == -1)
    {
        fprintf(stderr, "Error in bind(). %s\n", strerror(errno));
        exit(0);
    }
    
    int prev_seq=0;
    /* start send and receiving packets */
    int FINsent = 0;
    while(1)
    {
        /* Receive packet */
        char* buffer[525];
        struct PACKET* pkt_recv = malloc(sizeof(struct PACKET));
        int byte_recv = recvfrom(sockfd, buffer, 524, 0, (struct sockaddr*)&cli_addr, &cli_addr_len);
        if (byte_recv< 0)
            continue;
        if (byte_recv == 0)
        {
            if (SYNACK_TIME>0 && m_time() - 500 > SYNACK_TIME)
            {
                printf("TIMEOUT %d\n", BEGIN_SEQ);
                struct PACKET* synack;
                synack = add_pkt(1, 1, 0, 1, synack_acknum, BEGIN_SEQ);
                printf("RESEND %d SYN ACK\n", BEGIN_SEQ);
                send_pkt(sockfd, synack, 12, 0, (struct sockaddr*)&cli_addr, sizeof(cli_addr));
                SYNACK_TIME = m_time();
                free(synack);
            }
            else if (ACK_srv_TIME>0 && m_time()-500 > ACK_srv_TIME)
            {
                printf("TIMEOUT %d\n", fin_seqnum);
                struct PACKET* ack;
                ack = add_pkt(1, 0, 0, 1, fin_acknum, fin_seqnum);
                printf("RESEND %d ACK\n", fin_seqnum);
                send_pkt(sockfd, ack, 12, 0, (struct sockaddr*)&cli_addr, sizeof(cli_addr));
                ACK_srv_TIME = m_time();
                free(ack);
            }
            else if (FIN_srv_TIME>0 && m_time() - 500 > FIN_srv_TIME)
            {
                printf("TIMEOUT %d\n", fin_seqnum);
                struct PACKET* fin;
                fin = add_pkt(0, 0, 1, 0, 0, fin_seqnum);
                printf("RESEND %d FIN\n", fin_seqnum);
                send_pkt(sockfd, fin, 12, 0, (struct sockaddr*)&cli_addr, sizeof(cli_addr));
                FIN_srv_TIME = m_time();
                free(fin);
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
        //printf("************ %d\n", byte_recv);
        
        /* 3-way handshake */
        if (pkt_recv->SYN == 1)
        {
            SYN_TIME = -1;
            expected_seq = (pkt_recv->seq_num + 1) % (MAXSEQ + 1);
            FINsent = 0;
            int new_ack = (pkt_recv->seq_num + 1) % (MAXSEQ + 1);
            int new_seq = BEGIN_SEQ;
            struct PACKET* synack;
            synack = add_pkt(1, 1, 0, 1, new_ack, new_seq);
            send_pkt(sockfd, synack, 524, 0, (struct sockaddr*)&cli_addr, sizeof(cli_addr));
            printf("SEND %d %d SYN ACK\n", synack->seq_num, synack->ack_num);
            
            prev_seq = new_seq;
            
            connection_order += 1;
            char nameOfRecvFile[10];
            sprintf(nameOfRecvFile, "%d.file", connection_order);
            fp = fopen(nameOfRecvFile, "wb");
            fseek(fp, 0, SEEK_SET);
            if (fp == NULL)
            {
                fprintf(stderr, "Error in fopen().\n");
                exit(0);
            }
            synack_acknum = synack->ack_num;
            free(synack);
            SYNACK_TIME = m_time();
            
            
        }
        
        /* 4-way FIN */
        else if (pkt_recv->FIN==1)
        {
            FIN_cli_TIME = -1;
            int new_ack = (pkt_recv->seq_num + 1) % (MAXSEQ + 1);
            int new_seq = prev_seq;
            
            fin_acknum = new_ack;
            fin_seqnum = new_seq;
            
            struct PACKET *ack, *fin;
            ack = add_pkt(1, 0, 0, 1, new_ack, new_seq);
            new_ack = 0;
            fin = add_pkt(0, 0, 1, 0, 0, new_seq);
            send_pkt(sockfd, ack, 524, 0, (struct sockaddr*)&cli_addr, sizeof(cli_addr));
            printf("SEND %d %d ACK\n", ack->seq_num, ack->ack_num);
            sendto(sockfd, fin, 524, 0, (struct sockaddr*)&cli_addr, sizeof(cli_addr));
            printf("SEND %d %d FIN\n", fin->seq_num, fin->ack_num);
            FINsent = 1;
            if (fp == NULL)
            {
                fprintf(stderr, "Error in recording files!\n");
                exit(1);
            }
            fclose(fp);
            free(ack);
            free(fin);
        }
        
        /* last ACK for 3-way handshake */
        else if (pkt_recv->ACK == 1 && FINsent == 0)
        {
            //sleep(2);
            PKT_SENT_TIME[0] = -1;
            int new_seq = pkt_recv->ack_num;
            int new_ack = pkt_recv->seq_num;
            struct PACKET* special_ack;
            special_ack = add_pkt(1, 0, 0, 1, new_ack, new_seq);
            send_pkt(sockfd, special_ack, 524, 0, (struct sockaddr*)&cli_addr, sizeof(cli_addr));
            printf("SEND %d %d ACK\n", special_ack->seq_num, special_ack->ack_num);
            prev_seq = special_ack->seq_num;
            if (byte_recv > 12)
                fwrite(pkt_recv->payload, sizeof(char), byte_recv-12, fp);
            expected_seq = pkt_recv->seq_num + byte_recv - 12;
            DONE_PKT = pkt_recv->div_num;
            free(special_ack);
        }
        
        /* regular packet with data */
        else if(FINsent == 0)
        {
            PKT_SENT_TIME[pkt_recv->div_num] = -1;
//            fprintf(stderr, "***** %ld\n ", PKT_SENT_TIME[pkt_recv->div_num]);
            int new_seq = prev_seq;
            int new_ack = (pkt_recv->seq_num + byte_recv - 12) % (MAXSEQ + 1);
    
            struct PACKET* normal_ack;
            normal_ack = add_pkt(1, 0, 0, 1, new_ack, new_seq);
            normal_ack->div_num = pkt_recv->div_num;
            
            /* Write to *.file */
            if (byte_recv > 12)
            {
                /* in-order */
                if (pkt_recv->div_num == DONE_PKT + 1)
                {
                    fwrite(pkt_recv->payload, sizeof(char), byte_recv-12, fp);
                    DONE_PKT = pkt_recv->div_num;
                    send_pkt(sockfd, normal_ack, 12, 0, (struct sockaddr*)&cli_addr, sizeof(cli_addr));
                    printf("SEND %d %d ACK\n", normal_ack->seq_num, normal_ack->ack_num);
//                    int k = pkt_recv->div_num;
//                    while(recv_buffer[k][0] != -2)
//                    {
//                        fwrite(recv_buffer[k], sizeof(char), 512, fp);
//                        k ++;
//                    }
                }
//                /* out-of-order: too fast */
//                else if (pkt_recv->div_num > DONE_PKT + 1)
//                {
//                    memcpy(recv_buffer[pkt_recv->div_num], pkt_recv->payload, byte_recv);
//                }
            }
            
            /* ack the client: I sucessfully received this pkt */
//            send_pkt(sockfd, normal_ack, 12, 0, (struct sockaddr*)&cli_addr, sizeof(cli_addr));
//            printf("SEND %d %d ACK\n", normal_ack->seq_num, normal_ack->ack_num);
            
            /* Update prev sequence number as current, for next iter */
            int plsz = byte_recv - 12;  // payload size
            prev_seq = (plsz==0) ? (pkt_recv->seq_num+1) : (pkt_recv->seq_num+plsz);
            prev_seq %= MAXSEQ + 1;
            
            free(normal_ack);
            
        }
        
        /* Free resources */
	// free(pkt_recv);
    }
    return 0;
}

/*****************************************************************/
/************************ END OF MAIN ****************************/
/*****************************************************************/

void debug(struct PACKET* pkt)
{
    fprintf(stderr, "**********\n");
    fprintf(stderr, "SEQ_no:%d ACK_no:%d ACK:%d SYN:%d FIN:%d FLG:%d\n", pkt->seq_num, pkt->ack_num, pkt->ACK, pkt->SYN, pkt->FIN, pkt->FLG);
    fprintf(stderr, "PAYLOAD:%s", pkt->payload);
    fprintf(stderr, "**********\n");
}

void send_pkt(int sockfd, const void *buffer, size_t len, int flags,  const struct sockaddr* dest_addr, socklen_t addr_len)
{
    sendto(sockfd, buffer, len, flags, dest_addr, addr_len);
}
