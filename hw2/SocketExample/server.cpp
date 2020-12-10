#include <iostream>
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h> 
#include <string.h>
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

void *doInChildThread(void *ptr);

int main(int argc, char** argv){

    int localSocket, remoteSocket, port = atoi(argv[1]), cnt = 0;                               

    struct  sockaddr_in localAddr,remoteAddr;
          
    int addrLen = sizeof(struct sockaddr_in);

    pthread_t pid[10];  

    // create folder
    int check;
    check = mkdir("./server_dir", 0777); 
    if (check == -1) 
        cerr << "Error :  " << strerror(errno) << endl; 
    else
        cout << "Directory created" << endl;

    localSocket = socket(AF_INET , SOCK_STREAM , 0);
    
    if (localSocket == -1){
        printf("socket() call failed!!");
        return 0;
    }

    localAddr.sin_family = AF_INET;
    localAddr.sin_addr.s_addr = INADDR_ANY;
    localAddr.sin_port = htons(port);

      
    if( bind(localSocket,(struct sockaddr *)&localAddr , sizeof(localAddr)) < 0) {
        printf("Can't bind() socket");
        return 0;
    }
        
    listen(localSocket , 3);

    while(1){    
        std::cout <<  "Waiting for connections...\n"
                <<  "Server Port:" << port << std::endl;

        remoteSocket = accept(localSocket, (struct sockaddr *)&remoteAddr, (socklen_t*)&addrLen);  
        
        if (remoteSocket < 0) {
            printf("accept failed!");
            return 0;
        }
                
        std::cout << "Connection accepted" << std::endl;

        pthread_create(&pid[cnt], NULL, doInChildThread, &remoteSocket);
        cnt += 1;
        
    }

    for(int i = 0; i < cnt; i++) 
        pthread_join(pid[i],NULL);

    return 0;
}

void *doInChildThread(void *ptr) {
    int remoteSocket = *(int *)ptr;
    int sent, recved;

    Msg recv_msg = {.flag = 0, .nbytes = 0, .buf = {}};
    Msg send_msg = {.flag = 0, .nbytes = 0, .buf = {}};
    char dir_buf[BUFF_SIZE] = {};
    char tmp_buf[BUFF_SIZE] = {};
    char filename[BUFF_SIZE] = {};

    bzero(send_msg.buf, sizeof(char)*BUFF_SIZE);
    strcpy(send_msg.buf,"Connection Successful!!\n");
    send(remoteSocket, &send_msg, sizeof(Msg), 0);

    while(1) {
        bzero(recv_msg.buf, sizeof(char)*BUFF_SIZE);
        recv_msg.flag = 0;
        send_msg.flag = 0;
        if((recved = recv(remoteSocket, &recv_msg, sizeof(Msg), 0)) > 0) {
                
            vector<string> input_vec;
            char* message = new char[strlen(recv_msg.buf)+1];
            strcpy(message, recv_msg.buf);
            char* tmp_str = strtok(message, " ");
            while(tmp_str != NULL) {
                input_vec.push_back(string(tmp_str));
                tmp_str = strtok(NULL, " ");
            }
            
            if(strcmp(recv_msg.buf, "exit") == 0)
                break;

            else if(strcmp(recv_msg.buf, "ls") == 0) {
                bzero(dir_buf,sizeof(char)*BUFF_SIZE);
                struct dirent *pDirent;
                DIR *pDir;
                pDir = opendir("./server_dir");
                while ((pDirent = readdir(pDir)) != NULL) {
                    if(strcmp(pDirent->d_name, ".") == 0)
                        continue;
                    if(strcmp(pDirent->d_name, "..") == 0)
                        continue; 
                    sprintf(tmp_buf, "%s ", pDirent->d_name);
                    strcat(dir_buf, tmp_buf);
                }
                closedir (pDir);

                if(strlen(dir_buf) == 0)
                    strcpy(dir_buf, " ");
                bzero(send_msg.buf, sizeof(char)*BUFF_SIZE);
                strcpy(send_msg.buf, dir_buf);
                send(remoteSocket, &send_msg, sizeof(Msg), 0);
            }

            else if((strcmp(input_vec[0].c_str(), "put") == 0) && (input_vec.size() == 2)) {  
                bzero(recv_msg.buf, sizeof(char)*BUFF_SIZE);
                if((recved = recv(remoteSocket, &recv_msg, sizeof(Msg), 0)) > 0) {
                    if(strcmp(recv_msg.buf, "file exists") == 0) {
                        bzero(filename,sizeof(char)*BUFF_SIZE);
                        strcpy(filename, "./server_dir/");
                        strcat(filename, input_vec[1].c_str());

                        FILE *fp = fopen(filename, "wb");
                        bzero(recv_msg.buf, sizeof(char)*BUFF_SIZE);
                        int nbytes, sum = 0;
                        while((nbytes = recv(remoteSocket, &recv_msg, sizeof(Msg), 0)) > 0) {
                            if (recv_msg.flag == 1)
                                break;
                            sum += recv_msg.nbytes;
                            printf("sum = %d\n", sum);
                            fwrite(recv_msg.buf, sizeof(char), recv_msg.nbytes, fp);
                            bzero(recv_msg.buf, sizeof(char)*BUFF_SIZE);
                        }
                        fclose(fp);

                        bzero(send_msg.buf, sizeof(char)*BUFF_SIZE);
                        strcpy(send_msg.buf, "File uploading complete.");
                        send(remoteSocket, &send_msg, sizeof(Msg), 0);
                    }
                    else {
                        bzero(send_msg.buf, sizeof(char)*BUFF_SIZE);
                        strcpy(send_msg.buf, "The file doesn't exist.");
                        send(remoteSocket, &send_msg, sizeof(Msg), 0);
                    }
                }
            }

            else if(strcmp(input_vec[0].c_str(), "get") == 0) {
                if(input_vec.size() == 2) {
                    int flag = 0;
                    struct dirent *pDirent;
                    DIR *pDir;
                    pDir = opendir("./server_dir");
                    while ((pDirent = readdir(pDir)) != NULL) {
                        if(strcmp(pDirent->d_name, input_vec[1].c_str()) == 0) {
                            flag = 1;
                            break;
                        }
                    }
                    closedir(pDir);

                    if(flag == 1) {
                        bzero(send_msg.buf, sizeof(char)*BUFF_SIZE);
                        strcpy(send_msg.buf, "file exists");
                        send(remoteSocket, &send_msg, sizeof(Msg), 0);

                        bzero(filename, sizeof(char)*BUFF_SIZE);
                        strcpy(filename, "./server_dir/");
                        strcat(filename, input_vec[1].c_str());

                        FILE *fp = fopen(filename, "rb");
                        bzero(send_msg.buf, sizeof(char)*BUFF_SIZE);
                        int nbytes, sum;
                        while((nbytes = fread(send_msg.buf, sizeof(char), BUFF_SIZE, fp)) > 0) {
                            send_msg.nbytes = nbytes;
                            send(remoteSocket, &send_msg, sizeof(Msg), 0);
                            bzero(send_msg.buf, sizeof(char)*BUFF_SIZE);
                        }
                        fclose(fp);
                        
                        send_msg.flag = 1;
                        bzero(send_msg.buf, sizeof(char)*BUFF_SIZE);
                        send(remoteSocket, &send_msg, sizeof(Msg), 0);
                        strcpy(send_msg.buf, "File download complete.");
                        send(remoteSocket, &send_msg, sizeof(Msg), 0);
                    }
                    else {
                        bzero(send_msg.buf, sizeof(char)*BUFF_SIZE);
                        strcpy(send_msg.buf, "The file doesn't exist.");
                        send(remoteSocket, &send_msg, sizeof(Msg), 0);
                        send(remoteSocket, &send_msg, sizeof(Msg), 0);
                    }
                }

                else {
                    bzero(send_msg.buf, sizeof(char)*BUFF_SIZE);
                    strcpy(send_msg.buf, "Command format error.");
                    send(remoteSocket, &send_msg, sizeof(Msg), 0);
                }
            }

        
            else if(strcmp(input_vec[0].c_str(), "play") == 0) {
                if(input_vec.size() == 2) {
                    int flag = 0;
                    struct dirent *pDirent;
                    DIR *pDir;
                    pDir = opendir("./server_dir");
                    while ((pDirent = readdir(pDir)) != NULL) {
                        if(strcmp(pDirent->d_name, input_vec[1].c_str()) == 0) {
                            flag = 1;
                            break;
                        }
                    }
                    closedir(pDir);

                    if(flag == 1) {
                        bzero(filename,sizeof(char)*BUFF_SIZE);
                        strcpy(filename, "./server_dir/");
                        strcat(filename, input_vec[1].c_str());

                        VideoCapture cap(filename);
                        int width  = cap.get(CV_CAP_PROP_FRAME_WIDTH);
                        int height = cap.get(CV_CAP_PROP_FRAME_HEIGHT);

                        bzero(send_msg.buf, sizeof(char)*BUFF_SIZE);
                        send_msg.flag = width;
                        send_msg.nbytes = height;
                        strcpy(send_msg.buf, "file exists");
                        send(remoteSocket, &send_msg, sizeof(Msg), 0);

                        Mat imgServer;
                        imgServer = Mat::zeros(height, width, CV_8UC3);   
                        int imgSize = imgServer.total() * imgServer.elemSize();
                        
                        if (!imgServer.isContinuous()) {
                            imgServer = imgServer.clone();
                        }
                        
                        typedef struct {
                            int flag;
                            uchar *iptr;
                        } Frame;

                        int nbytes;
                        Frame send_frame = {.flag = 0, .iptr = imgServer.data};

                        while(1) {      
                            cap >> imgServer;
                            cout << send_frame.iptr << endl;
                            nbytes = send(remoteSocket, &send_frame, sizeof(Frame), 0);
                            if((recved = recv(remoteSocket, &recv_msg, sizeof(Msg), MSG_DONTWAIT)) > 0) {
                                send_frame.flag = 1;
                                send(remoteSocket, &send_frame, sizeof(Frame), 0);
                                break;
                            }
                        }
                        cap.release();

                        bzero(send_msg.buf, sizeof(char)*BUFF_SIZE);
                        strcpy(send_msg.buf, "Video play complete.");
                        send(remoteSocket, &send_msg, sizeof(Msg), 0);
                        
                    }
                    else {
                        bzero(send_msg.buf, sizeof(char)*BUFF_SIZE);
                        strcpy(send_msg.buf, "The file doesn't exist.");
                        send(remoteSocket, &send_msg, sizeof(Msg), 0);
                        send(remoteSocket, &send_msg, sizeof(Msg), 0);
                    }
                }

                else {
                    bzero(send_msg.buf, sizeof(char)*BUFF_SIZE);
                    strcpy(send_msg.buf, "Command format error.");
                    send(remoteSocket, &send_msg, sizeof(Msg), 0);
                }
            }
    

            else{
                bzero(send_msg.buf,sizeof(char)*BUFF_SIZE);
                strcpy(send_msg.buf, "Command not found.");
                send(remoteSocket, &send_msg, sizeof(Msg), 0);
            }
        }
    }        
    close(remoteSocket);
    return 0;
}


