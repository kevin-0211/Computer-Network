#include <iostream>
#include <sys/socket.h> 
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h> 
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <dirent.h>
#include <vector>
#include <pthread.h>
#include <errno.h>
#include "opencv2/opencv.hpp"

#define BUFF_SIZE 1024

using namespace std;
using namespace cv;

typedef struct {
    int flag;
    int nbytes;
    char buf[BUFF_SIZE];
} Msg;

int main(int argc , char *argv[])
{   
    vector<string> arg;
    char* tmp_str = strtok(argv[1], ":");
    while(tmp_str != NULL) {
        arg.push_back(string(tmp_str));
        tmp_str = strtok(NULL, " ");
    }
    if(arg.size() != 2) {
        printf("Format error.\n");
        return 0;
    }

    char serverIP[BUFF_SIZE] = {};
    strcpy(serverIP, arg[0].c_str());
    int serverPort = atoi(arg[1].c_str());
    int localSocket, recved;
    localSocket = socket(AF_INET , SOCK_STREAM , 0);

    if (localSocket == -1){
        printf("Fail to create a socket.\n");
        return 0;
    }

    struct sockaddr_in info;
    bzero(&info,sizeof(info));

    info.sin_family = PF_INET;
    info.sin_addr.s_addr = inet_addr(serverIP);
    info.sin_port = htons(serverPort);


    int err = connect(localSocket,(struct sockaddr *)&info,sizeof(info));
    if(err==-1){
        printf("Connection error\n");
        return 0;
    }

    Msg recv_msg = {.flag = 0, .nbytes = 0, .buf = {}};
    Msg send_msg = {.flag = 0, .nbytes = 0, .buf = {}};
    char input[BUFF_SIZE] = {};
    char filename[BUFF_SIZE] = {};
    int sent;
    
    while(1){
        bzero(recv_msg.buf, sizeof(char)*BUFF_SIZE);
        if((recved = recv(localSocket, &recv_msg, sizeof(Msg), 0)) < 0){
            cout << "recv failed, with received bytes = " << recved << endl;
            break;
        }
        else if(recved == 0){
            cout << "<end>\n";
            break;
        }
        printf("%s\n", recv_msg.buf);
        
        while(1){
            bzero(input,sizeof(char)*BUFF_SIZE);
            bzero(send_msg.buf,sizeof(char)*BUFF_SIZE);
            recv_msg.flag = 0;
            send_msg.flag = 0;
            cin.getline(input, BUFF_SIZE);
            if (strlen(input) == 0)
                continue;
            strcpy(send_msg.buf, input);
            send(localSocket, &send_msg, sizeof(Msg), 0);

            vector<string> input_vec;
            char* tmp_input = new char[strlen(input)+1];
            strcpy(tmp_input, input);
            char* tmp_str = strtok(tmp_input, " ");
            while(tmp_str != NULL) {
                input_vec.push_back(string(tmp_str));
                tmp_str = strtok(NULL, " ");
            }

            if(strcmp(input, "exit") == 0)
                break;

            else if((strcmp(input_vec[0].c_str(), "put") == 0) && input_vec.size() == 2){              
                int flag = 0;
                struct dirent *pDirent;
                DIR *pDir;
                pDir = opendir("./client_dir");
                while((pDirent = readdir(pDir)) != NULL) {
                    if(strcmp(pDirent->d_name, input_vec[1].c_str()) == 0) {
                        flag = 1;
                        break;
                    }
                }
                closedir (pDir);

                if(flag == 1) {
                    bzero(send_msg.buf, sizeof(char)*BUFF_SIZE);
                    strcpy(send_msg.buf, "file exists");
                    send(localSocket, &send_msg, sizeof(Msg), 0);

                    bzero(filename,sizeof(char)*BUFF_SIZE);
                    strcpy(filename, "./client_dir/");
                    strcat(filename, input_vec[1].c_str());
                    FILE *fp = fopen(filename, "rb");
                    int nbytes;

                    bzero(send_msg.buf, sizeof(char)*BUFF_SIZE);
                    while((nbytes = fread(send_msg.buf, sizeof(char), BUFF_SIZE, fp)) > 0) {
                        send_msg.nbytes = nbytes;
                        send(localSocket, &send_msg, sizeof(Msg), 0);
                        bzero(send_msg.buf, sizeof(char)*BUFF_SIZE);
                    }
                    fclose(fp);
                    send_msg.flag = 1;
                    bzero(send_msg.buf, sizeof(char)*BUFF_SIZE);
                    send(localSocket, &send_msg, sizeof(Msg), 0);
                }
                else {
                    bzero(send_msg.buf, sizeof(char)*BUFF_SIZE);
                    strcpy(send_msg.buf, "file doesn't exist");
                    send(localSocket, &send_msg, sizeof(Msg), 0);
                }
            }

            else if((strcmp(input_vec[0].c_str(), "get") == 0) && input_vec.size() == 2) {
                bzero(recv_msg.buf, sizeof(char)*BUFF_SIZE);
                if((recved = recv(localSocket, &recv_msg, sizeof(Msg), 0)) > 0) {
                    if(strcmp(recv_msg.buf, "file exists") == 0) {
                        bzero(filename, sizeof(char)*BUFF_SIZE);
                        strcpy(filename, "./client_dir/");
                        strcat(filename, input_vec[1].c_str());

                        FILE *fp = fopen(filename, "wb");
                        bzero(recv_msg.buf, sizeof(char)*BUFF_SIZE);
                        int nbytes, sum = 0;
                        while((nbytes = recv(localSocket, &recv_msg, sizeof(Msg), 0)) > 0) {
                            if(recv_msg.flag == 1)
                                break;
                            sum += recv_msg.nbytes;
                            printf("sum = %d\n", sum);
                            fwrite(recv_msg.buf, sizeof(char), recv_msg.nbytes, fp);
                            bzero(recv_msg.buf, sizeof(char)*BUFF_SIZE);
                        }
                        fclose(fp);
                    }
                }
            }

            else if((strcmp(input_vec[0].c_str(), "play") == 0) && input_vec.size() == 2) {
                bzero(recv_msg.buf, sizeof(char)*BUFF_SIZE);
                if((recved = recv(localSocket, &recv_msg, sizeof(Msg), 0)) > 0) {
                    if(strcmp(recv_msg.buf, "file exists") == 0) {
                        int width = recv_msg.flag;
                        int height = recv_msg.nbytes;

                        Mat imgClient;
                        imgClient = Mat::zeros(height, width, CV_8UC3);

                        int nbytes;
                        int imgSize = imgClient.total() * imgClient.elemSize();

                        if(!imgClient.isContinuous()){
                            imgClient = imgClient.clone();
                        }

                        uchar *iptr = imgClient.data;

                        while(1) {
                            recv(localSocket, iptr, imgSize, MSG_WAITALL);
                            recv(localSocket, &recv_msg, sizeof(Msg), 0);
                            if(recv_msg.flag == 1)
                                break;
                            imshow("Video", imgClient); 
                          
                            char c = (char)waitKey(33.3333);
                            if(c == 27) {
                                bzero(send_msg.buf, sizeof(char)*BUFF_SIZE);
                                send(localSocket, &send_msg, sizeof(Msg), 0);
                                break;
                            }
                        }   
                        destroyAllWindows();

                        if(recv_msg.flag == 0) {
                            while(1) {
                                recv(localSocket, iptr, imgSize, MSG_WAITALL);
                                recv(localSocket, &recv_msg, sizeof(Msg), 0);
                                if(recv_msg.flag == 1)
                                    break;
                            }
                        }
                    }
                }
            }

            bzero(recv_msg.buf, sizeof(char)*BUFF_SIZE);
            if ((recved = recv(localSocket, &recv_msg, sizeof(Msg), 0)) > 0)  
                printf("%s\n", recv_msg.buf);
        }
    }

    printf("close Socket\n");
    close(localSocket);
    return 0;
}

