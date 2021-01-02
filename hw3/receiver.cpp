#include <iostream>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include "opencv2/opencv.hpp"

using namespace std;
using namespace cv;

typedef struct {
	int length;
	int seqNumber;
	int ackNumber;
	int fin;
	int syn;
	int ack;
} header;

typedef struct{
	header head;
	char data[4096];
} segment;

void setIP(char *dst, char *src) {
    if(strcmp(src, "0.0.0.0") == 0 || strcmp(src, "local") == 0
            || strcmp(src, "localhost")) {
        sscanf("127.0.0.1", "%s", dst);
    } else {
        sscanf(src, "%s", dst);
    }
}

int main(int argc, char* argv[]){
    int receiversocket, portNum, nBytes;
    float loss_rate;
    segment s_tmp;
    struct sockaddr_in sender, agent, receiver, tmp_addr;
    socklen_t agent_size, tmp_size;
    char ip[3][50];
    int port[3], i;
    
    if(argc != 4){
        fprintf(stderr,"用法: %s <agent IP> <agent port> <recv port>\n", argv[0]);
        fprintf(stderr, "例如: ./agent local 8888 8889\n");
        exit(1);
    } else {
        setIP(ip[0], argv[1]);
        setIP(ip[1], (char *)"local");

        sscanf(argv[2], "%d", &port[0]);
        sscanf(argv[3], "%d", &port[1]);
    }

    /*Create UDP socket*/
    receiversocket = socket(PF_INET, SOCK_DGRAM, 0);

    /*Configure settings in agent struct*/
    agent.sin_family = AF_INET;
    agent.sin_port = htons(port[0]);
    agent.sin_addr.s_addr = inet_addr(ip[0]);
    memset(agent.sin_zero, '\0', sizeof(agent.sin_zero));    


    /*Configure settings in receiver struct*/
    receiver.sin_family = AF_INET;
    receiver.sin_port = htons(port[1]);
    receiver.sin_addr.s_addr = inet_addr(ip[1]);
    memset(receiver.sin_zero, '\0', sizeof(receiver.sin_zero)); 

    /*bind socket*/
    bind(receiversocket,(struct sockaddr *)&receiver,sizeof(receiver));

    /*Initialize size variable to be used later on*/
    agent_size = sizeof(agent);
    tmp_size = sizeof(tmp_addr);

    printf("可以開始測囉^Q^\n");
    printf("agent info: ip = %s port = %d\n", ip[0], port[0]);
    printf("receiver info: ip = %s port = %d\n", ip[1], port[1]);


    int segment_size, index;
    srand(time(NULL));

    FILE *fp = fopen("./recv/test.mpg", "wb");
    
    Mat imgClient;
    imgClient = Mat::zeros(height, width, CV_8UC3);

    int imgSize = imgClient.total() * imgClient.elemSize();

    if(!imgClient.isContinuous()){
        imgClient = imgClient.clone();
    }
    printf("height = %d, width = %d\n", height, width);
    int nbytes, cnt = 1, num = 0;
    while(1){
        /*Receive message from receiver and sender*/
        memset(&s_tmp, 0, sizeof(s_tmp));
        segment_size = recvfrom(receiversocket, &s_tmp, sizeof(s_tmp), 0, (struct sockaddr *)&tmp_addr, &tmp_size);
        if(segment_size > 0){
            if (s_tmp.head.fin == 1) {
                printf("recv	fin\n");
                memset(&s_tmp, 0, sizeof(s_tmp));
                s_tmp.head.ack = 1;
                s_tmp.head.fin = 1;
                sendto(receiversocket, &s_tmp, sizeof(s_tmp), 0, (struct sockaddr *)&agent, agent_size);
                printf("send	finack\n");
                break;
            }
            cnt = s_tmp.head.seqNumber;
            if (cnt == num + 1) {
                fwrite(s_tmp.data, sizeof(char), s_tmp.head.length, fp);
                printf("recv	data	#%d\n", cnt);
                memset(&s_tmp, 0, sizeof(s_tmp));
                s_tmp.head.ack = 1;
                s_tmp.head.fin = 0;
                s_tmp.head.ackNumber = cnt;
                sendto(receiversocket, &s_tmp, sizeof(s_tmp), 0, (struct sockaddr *)&agent, agent_size);
                printf("send	ack 	#%d\n", s_tmp.head.ackNumber);
                num = cnt;
            }
            else {
                printf("drop	data	#%d\n", cnt);
                memset(&s_tmp, 0, sizeof(s_tmp));
                s_tmp.head.ack = 1;
                s_tmp.head.fin = 0;
                s_tmp.head.ackNumber = num;
                sendto(receiversocket, &s_tmp, sizeof(s_tmp), 0, (struct sockaddr *)&agent, agent_size);
                printf("send	ack 	#%d\n", s_tmp.head.ackNumber);
            }
        }
    }
    fclose(fp);
    return 0;
}