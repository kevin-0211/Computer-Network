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
	uchar data[4096];
} segment;

void setIP(char *dst, char *src) {
    if(strcmp(src, "0.0.0.0") == 0 || strcmp(src, "local") == 0
            || strcmp(src, "localhost")) {
        sscanf("127.0.0.1", "%s", dst);
    } else {
        sscanf(src, "%s", dst);
    }
}

int main(int argc, char* argv[]) {
    int sendersocket, portNum, nBytes;
    segment s_tmp;
    struct sockaddr_in sender, agent, tmp_addr;
    socklen_t agent_size, tmp_size;
    char ip[2][50];
    char filename[50];
    int port[2];
    
    if(argc != 5){
        fprintf(stderr,"用法: %s <agent IP> <sender port> <agent port> <path of file>\n", argv[0]);
        fprintf(stderr, "例如: ./server local 8887 8888 tmp.mpg\n");
        exit(1);
    } else {
        setIP(ip[0], (char *)"local"); // sender IP
        setIP(ip[1], argv[1]); // agent IP

        sscanf(argv[2], "%d", &port[0]); // sender port
        sscanf(argv[3], "%d", &port[1]); // agent port

        sscanf(argv[4], "%s", filename);
    }

    /*Create UDP socket*/
    sendersocket = socket(PF_INET, SOCK_DGRAM, 0);

    /*Configure settings in sender struct*/
    sender.sin_family = AF_INET;
    sender.sin_port = htons(port[0]);
    sender.sin_addr.s_addr = inet_addr(ip[0]);
    memset(sender.sin_zero, '\0', sizeof(sender.sin_zero));  

    /*Configure settings in agent struct*/
    agent.sin_family = AF_INET;
    agent.sin_port = htons(port[1]);
    agent.sin_addr.s_addr = inet_addr(ip[1]);
    memset(agent.sin_zero, '\0', sizeof(agent.sin_zero));    

    /*bind socket*/
    bind(sendersocket,(struct sockaddr *)&sender,sizeof(sender));

    /*Initialize size variable to be used later on*/
    agent_size = sizeof(agent);
    tmp_size = sizeof(tmp_addr);

    printf("可以開始測囉^Q^\n");
    printf("sender info: ip = %s port = %d\n",ip[0], port[0]);
    printf("agent info: ip = %s port = %d\n", ip[1], port[1]);
   
    int segment_size, index;
    srand(time(NULL));
       
    VideoCapture cap(filename);
    if(cap.isOpened() == false) {
        cout << "cannot open the video file" <<endl;
        return -1;
    }

    int width = cap.get(CV_CAP_PROP_FRAME_WIDTH);
    int height = cap.get(CV_CAP_PROP_FRAME_HEIGHT);
    Mat imgServer;
    imgServer = Mat::zeros(height, width, CV_8UC3);
    int imgSize = imgServer.total() * imgServer.elemSize();

    memset(&s_tmp, 0, sizeof(s_tmp));
    s_tmp.head.seqNumber = height;
    s_tmp.head.ackNumber = width;
    sendto(sendersocket, &s_tmp, sizeof(s_tmp), 0, (struct sockaddr *)&agent, agent_size);
    
    if (!imgServer.isContinuous()) {
        imgServer = imgServer.clone();
    }

    int recv, flag, cnt = 1, window = 1, threshold = 16, num = 0, i, j;
    int frame = imgSize / 4096 + 1, rest = imgSize - (frame - 1) * 4096;
    
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 1000;
    if (setsockopt(sendersocket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("Error");
    }

    while (1) {
        uchar *buf = new uchar[imgSize];
        cap >> imgServer;
        if (imgServer.empty())
            break;
        memcpy(buf, imgServer.data, imgSize);
       
        flag = 0;
        while (1) {
            for (i = 0; i < window; i++) {
                if (flag == 1)
                    break;
                memset(&s_tmp, 0, sizeof(s_tmp));
                if (cnt % frame == 0) {
                    memcpy(&s_tmp.data, &buf[((cnt-1)%frame)*4096], rest);
                    s_tmp.head.length = rest;
                }
                else {
                    memcpy(&s_tmp.data, &buf[((cnt-1)%frame)*4096], 4096);   
                    s_tmp.head.length = 4096;
                }
                if (cnt % frame == 0 || i == window-1)
                    s_tmp.head.syn = 1;
                else
                    s_tmp.head.syn = 0;
                s_tmp.head.seqNumber = cnt;
                s_tmp.head.fin = 0;
                s_tmp.head.ack = 0;
                sendto(sendersocket, &s_tmp, sizeof(s_tmp), 0, (struct sockaddr *)&agent, agent_size);
                printf("send	data	#%d,    winSize = %d\n", cnt, window);
                if (cnt % frame == 0)
                    flag = 1;
                cnt += 1;
            }
            for (j = 0; j < i; j++) {
                memset(&s_tmp, 0, sizeof(s_tmp));
                recv = recvfrom(sendersocket, &s_tmp, sizeof(s_tmp), 0, (struct sockaddr *)&tmp_addr, &tmp_size);
                if (recv < 0)
                    break;
                printf("recv    ack	#%d\n", s_tmp.head.ackNumber);
                num = s_tmp.head.ackNumber;
            }
            if (j < i) {
                if (window / 2 > 1)
                    threshold = window / 2;
                else 
                    threshold = 1;
                printf("time    out,            threshold = %d\n", threshold);
                window = 1;
                cnt = num+1;
                flag = 0;
            }
            else {
                if (window < threshold)
                    window *= 2;
                else
                    window += 1;
                if (flag == 1)
                    break;
            }  
        }
    }
    cap.release();
    memset(&s_tmp, 0, sizeof(s_tmp));
    s_tmp.head.fin = 1;
    s_tmp.head.ack = 0;
    sendto(sendersocket, &s_tmp, sizeof(s_tmp), 0, (struct sockaddr *)&agent, agent_size);
    printf("send    fin\n");
    recvfrom(sendersocket, &s_tmp, sizeof(s_tmp), 0, (struct sockaddr *)&tmp_addr, &tmp_size);
    printf("recv    finack\n");
    return 0;
}