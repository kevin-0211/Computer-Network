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

#define BUFF_SIZE 1024

using namespace std;

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
    char Message[BUFF_SIZE] = {};
    char receiveMessage[BUFF_SIZE] = {};
    char dir_buf[BUFF_SIZE] = {};
    char tmp_buf[BUFF_SIZE] = {};
    char file_buf[BUFF_SIZE] = {};
    char dir_name[BUFF_SIZE] = {};

    strcpy(Message,"Hello World!!\n");
    sent = send(remoteSocket,Message,strlen(Message),0);

    while(1) {
        bzero(receiveMessage,sizeof(char)*BUFF_SIZE);
        if((recved = recv(remoteSocket,receiveMessage,sizeof(char)*BUFF_SIZE,0)) > 0) {
                
            vector<string> input_vec;
            char * message = new char[strlen(receiveMessage)+1];
            strcpy(message, receiveMessage);
            char* tmp_str = strtok(message, " ");
            while(tmp_str != NULL) {
                input_vec.push_back(string(tmp_str));
                tmp_str = strtok(NULL, " ");
            }
            
            if(strcmp(receiveMessage, "exit") == 0)
                break;

            else if(strcmp(receiveMessage, "ls") == 0) {
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
                bzero(Message,sizeof(char)*BUFF_SIZE);
                strcpy(Message, dir_buf);
                send(remoteSocket,Message,strlen(Message),0);
            }

            else if((strcmp(input_vec[0].c_str(), "put") == 0) && (input_vec.size() == 2)) {
                
                bzero(receiveMessage,sizeof(char)*BUFF_SIZE);
                if((recved = recv(remoteSocket,receiveMessage,sizeof(char)*BUFF_SIZE,0)) > 0) {
                    printf("message = %s\n", receiveMessage);
                    if(strcmp(receiveMessage, "file exists") == 0) {
                        
                        bzero(dir_name,sizeof(char)*BUFF_SIZE);
                        strcpy(dir_name, "./server_dir/");
                        strcat(dir_name, input_vec[1].c_str());

                        FILE *fp = fopen(dir_name, "wb");
                        bzero(file_buf,sizeof(char)*BUFF_SIZE);
                        int nbytes;
                        while((nbytes = recv(remoteSocket, file_buf, BUFF_SIZE, 0)) > 0) {
                            fwrite(file_buf, sizeof(char), nbytes, fp);
                            bzero(file_buf,sizeof(char)*BUFF_SIZE);
                            if (nbytes == 0 || nbytes != BUFF_SIZE)
                                break;
                        }
                        fclose(fp);

                        bzero(Message,sizeof(char)*BUFF_SIZE);
                        strcpy(Message, "File upload complete.");
                        send(remoteSocket,Message,strlen(Message),0);
                    }
                    else {
                        bzero(Message,sizeof(char)*BUFF_SIZE);
                        strcpy(Message, "The file doesn't exist.");
                        send(remoteSocket,Message,strlen(Message),0);
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
                        bzero(Message,sizeof(char)*BUFF_SIZE);
                        strcpy(Message, "file exists");
                        send(remoteSocket,Message,strlen(Message),0);

                        bzero(dir_name,sizeof(char)*BUFF_SIZE);
                        strcpy(dir_name, "./server_dir/");
                        strcat(dir_name, input_vec[1].c_str());

                        FILE *fp = fopen(dir_name, "rb");
                        bzero(file_buf,sizeof(char)*BUFF_SIZE);
                        int nbytes;
                        while((nbytes = fread(file_buf, sizeof(char), BUFF_SIZE, fp)) > 0) {
                            if(send(remoteSocket, file_buf, nbytes, 0) < 0) {
                                fprintf(stderr, "ERROR: Failed to send file %s. (errno = %d)\n", input_vec[1].c_str(), errno);
                                break;
                            }
                            bzero(file_buf, sizeof(char)*BUFF_SIZE);
                            if (nbytes == 0 || nbytes != BUFF_SIZE)
                                break;
                        }
                        fclose(fp);
                            
                        recv(remoteSocket,receiveMessage,sizeof(char)*BUFF_SIZE,0);
                        bzero(Message,sizeof(char)*BUFF_SIZE);
                        strcpy(Message, "File download complete.");
                        send(remoteSocket,Message,strlen(Message),0);
                    }
                    else {

                        bzero(Message,sizeof(char)*BUFF_SIZE);
                        strcpy(Message, "The file doesn't exist.");
                        send(remoteSocket,Message,strlen(Message),0);
                        recv(remoteSocket,receiveMessage,sizeof(char)*BUFF_SIZE,0);
                        send(remoteSocket,Message,strlen(Message),0);
                    }
                }

                else {
                    bzero(Message,sizeof(char)*BUFF_SIZE);
                    strcpy(Message, "Command format error.");
                    send(remoteSocket,Message,strlen(Message),0);
                }
            }

            else{
                bzero(Message,sizeof(char)*BUFF_SIZE);
                strcpy(Message, "Command not found.");
                send(remoteSocket,Message,strlen(Message),0);
            }
        }
    }        
    close(remoteSocket);
    return 0;
}


