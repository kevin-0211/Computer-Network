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

#define BUFF_SIZE 1024

using namespace std;
int main(int argc , char *argv[])
{   
    int serverPort = atoi(argv[1]);
    int localSocket, recved;
    localSocket = socket(AF_INET , SOCK_STREAM , 0);

    if (localSocket == -1){
        printf("Fail to create a socket.\n");
        return 0;
    }

    struct sockaddr_in info;
    bzero(&info,sizeof(info));

    info.sin_family = PF_INET;
    info.sin_addr.s_addr = inet_addr("127.0.0.1");
    info.sin_port = htons(serverPort);


    int err = connect(localSocket,(struct sockaddr *)&info,sizeof(info));
    if(err==-1){
        printf("Connection error\n");
        return 0;
    }

    char receiveMessage[BUFF_SIZE] = {};
    char input[BUFF_SIZE] = {};
    char Message[BUFF_SIZE] = {};
    char file_buf[BUFF_SIZE] = {};
    char dir_name[BUFF_SIZE] = {};
    int sent;
    
    while(1){
        bzero(receiveMessage,sizeof(char)*BUFF_SIZE);
        if ((recved = recv(localSocket,receiveMessage,sizeof(char)*BUFF_SIZE,0)) < 0){
            cout << "recv failed, with received bytes = " << recved << endl;
            break;
        }
        else if (recved == 0){
            cout << "<end>\n";
            break;
        }
        printf("%d:%s\n",recved,receiveMessage);
        
        while(1){
            bzero(input,sizeof(char)*BUFF_SIZE);
            bzero(Message,sizeof(char)*BUFF_SIZE);
            cin.getline(input, BUFF_SIZE);
            strcpy(Message, input);
            send(localSocket, Message, strlen(Message), 0);

            vector<string> input_vec;
            char* message = new char[strlen(input)+1];
            strcpy(message, input);
            char* tmp_str = strtok(message, " ");
            while(tmp_str != NULL) {
                input_vec.push_back(string(tmp_str));
                tmp_str = strtok(NULL, " ");
            }

            if(strcmp(input, "exit") == 0)
                break;

            if((strcmp(input_vec[0].c_str(), "put") == 0) && input_vec.size() == 2){              
                int flag = 0;
                struct dirent *pDirent;
                DIR *pDir;
                pDir = opendir("./client_dir");
                while ((pDirent = readdir(pDir)) != NULL) {
                    if(strcmp(pDirent->d_name, input_vec[1].c_str()) == 0) {
                        flag = 1;
                        break;
                    }
                }
                closedir (pDir);

                if(flag == 1) {
                    bzero(Message, sizeof(char)*BUFF_SIZE);
                    strcpy(Message, "file exists");
                    send(localSocket, Message, strlen(Message), 0);

                    bzero(dir_name,sizeof(char)*BUFF_SIZE);
                    strcpy(dir_name, "./client_dir/");
                    strcat(dir_name, input_vec[1].c_str());
                    FILE *fp = fopen(dir_name, "rb");


                    bzero(file_buf, sizeof(char)*BUFF_SIZE);
                    int nbytes;
                    while((nbytes = fread(file_buf, sizeof(char), BUFF_SIZE, fp)) > 0) {
                        if(send(localSocket, file_buf, nbytes, 0) < 0) {
                            fprintf(stderr, "ERROR: Failed to send file %s. (errno = %d)\n", input_vec[1].c_str(), errno);
                            break;
                        }
                        bzero(file_buf, sizeof(char)*BUFF_SIZE);
                        if (nbytes == 0 || nbytes != BUFF_SIZE)
                            break;
                    }
                    fclose(fp);
                }
                else {
                    bzero(Message, sizeof(char)*BUFF_SIZE);
                    strcpy(Message, "file doesn't exist");
                    send(localSocket, Message, strlen(Message), 0);
                }
            }

            if((strcmp(input_vec[0].c_str(), "get") == 0) && input_vec.size() == 2) {
                bzero(receiveMessage,sizeof(char)*BUFF_SIZE);
                if ((recved = recv(localSocket,receiveMessage,sizeof(char)*BUFF_SIZE,0)) > 0) {
                    if(strcmp(receiveMessage, "file exists") == 0) {
                        bzero(dir_name,sizeof(char)*BUFF_SIZE);
                        strcpy(dir_name, "./client_dir/");
                        strcat(dir_name, input_vec[1].c_str());

                        FILE *fp = fopen(dir_name, "wb");
                        bzero(file_buf,sizeof(char)*BUFF_SIZE);
                        int nbytes;
                        while((nbytes = recv(localSocket, file_buf, BUFF_SIZE, 0)) > 0) {
                            fwrite(file_buf, sizeof(char), nbytes, fp);
                            bzero(file_buf,sizeof(char)*BUFF_SIZE);
                            if (nbytes == 0 || nbytes != BUFF_SIZE)
                                break;
                        }
                        fclose(fp);
                        bzero(Message, sizeof(char)*BUFF_SIZE);
                        strcpy(Message, "download complete");
                        send(localSocket, Message, strlen(Message), 0);
                    }
                    else {
                        bzero(Message, sizeof(char)*BUFF_SIZE);
                        strcpy(Message, "file doesn't exist");
                        send(localSocket, Message, strlen(Message), 0);
                    }
                }
            }

            bzero(receiveMessage,sizeof(char)*BUFF_SIZE);
            if ((recved = recv(localSocket,receiveMessage,sizeof(char)*BUFF_SIZE,0)) > 0)  
                printf("%s\n", receiveMessage);
        }
    }

    printf("close Socket\n");
    close(localSocket);
    return 0;
}

