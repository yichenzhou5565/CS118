#include <stdint.h>

/* when pkt is sent by client: entry set to time sent
 when pkt received by server: entry set to -1 */
long PKT_SENT_TIME[20480];       /* 10 MB / 512 B = 20480 */

long SYN_TIME = 0;
long SYNACK_TIME = 0;

long FIN_cli_TIME = 0;
long ACK_srv_TIME = 0;
long FIN_srv_TIME = 0;
long ACK_cli_TIME = 0;

int FN, FZ;         /* number of data pkts, size of file to transfer */

struct PACKET
{
    /* Header */
    char ACK;
    char SYN;
    char FIN;
    char FLG;
    uint16_t ack_num;       /* 2 bytes */
    uint16_t seq_num;       /* 2 bytes */
    int div_num;            /* 4 bytes */
    //char padding[2];        /* padding */
    
    /* Payload (data) */
    char payload[512];
};

struct PACKET* add_pkt(char ACK, char SYN, char FIN, char FLG, uint16_t ack_num, uint16_t seq_num)
{
    struct PACKET* pkt = malloc(sizeof(struct PACKET));
    pkt->ACK = ACK;
    pkt->SYN = SYN;
    pkt->FIN = FIN;
    pkt->FLG = FLG;
    pkt->ack_num = ack_num;
    pkt->seq_num = seq_num;
    
    return pkt;
}

/* return current time in milli seconds */
long m_time()
{
    struct timespec* now = malloc(sizeof(struct timespec));
    clock_gettime(CLOCK_REALTIME, now);
    long ret = 1000 * now->tv_sec + now->tv_nsec / 1000000;
    free(now);
    return ret;
}
