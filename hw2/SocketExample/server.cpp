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
#include <errno.h>
#include <signal.h>
#include "opencv2/opencv.hpp"

#define BUFF_SIZE 1024

using namespace std;
using namespace cv;


typedef struct
{
    int flag;
    int nbytes;
    char buf[BUFF_SIZE];
} Msg;

int main(int argc, char **argv) {
    signal(SIGPIPE, SIG_IGN);

    int localSocket, remoteSocket, port = atoi(argv[1]), cnt = 0, max_fd;

    struct sockaddr_in localAddr, remoteAddr;

    int addrLen = sizeof(struct sockaddr_in);

    pthread_t pid[10];

    fd_set read_fd, r;

    int sent, recved;

    Msg recv_msg = {.flag = 0, .nbytes = 0, .buf = {}};
    Msg send_msg = {.flag = 0, .nbytes = 0, .buf = {}};
    char dir_buf[BUFF_SIZE] = {};
    char tmp_buf[BUFF_SIZE] = {};
    char filename[BUFF_SIZE] = {};

    // create folder
    int check;
    check = mkdir("./server_dir", 0777);
    if (check == -1)
        cerr << "Error :  " << strerror(errno) << endl;
    else
        cout << "Directory created" << endl;

    localSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (localSocket == -1) {
        printf("socket() call failed!!");
        return 0;
    }

    localAddr.sin_family = AF_INET;
    localAddr.sin_addr.s_addr = INADDR_ANY;
    localAddr.sin_port = htons(port);

    if (bind(localSocket, (struct sockaddr *)&localAddr, sizeof(localAddr)) < 0) {
        printf("Can't bind() socket");
        return 0;
    }

    listen(localSocket, 3);

    FD_ZERO(&read_fd);
    FD_SET(localSocket, &read_fd);
    max_fd = localSocket;

    std::cout << "Waiting for connections...\n"
              << "Server Port:" << port << std::endl;

    while (1) {
        r = read_fd;
        select(max_fd+1, &r, NULL, NULL, NULL);
        for(int i = 0; i <= max_fd; i++) {
            if(FD_ISSET(i, &r)) {
                if(i == localSocket) {
                    remoteSocket = accept(localSocket, (struct sockaddr *)&remoteAddr, (socklen_t *)&addrLen);
                    if (remoteSocket < 0) {
                        printf("accept failed!");
                        return 0;
                    }
                    std::cout << "Connection accepted" << std::endl;
                    printf("remoteSocket = %d\n", remoteSocket);
                    bzero(send_msg.buf, sizeof(char) * BUFF_SIZE);
                    strcpy(send_msg.buf, "Connection Successful!!\n");
                    if ((sent = send(remoteSocket, &send_msg, sizeof(Msg), 0)) <= 0) {
                        printf("Client disconnected\n");
                        continue;
                    }

                    FD_SET(remoteSocket, &read_fd);
                    if(remoteSocket > max_fd)
                        max_fd = remoteSocket;

                    std::cout << "Waiting for connections...\n"
                              << "Server Port:" << port << std::endl;
                }
                else {
                    bzero(recv_msg.buf, sizeof(char) * BUFF_SIZE);
                    recv_msg.flag = 0;
                    send_msg.flag = 0;
                    if ((recved = recv(i, &recv_msg, sizeof(Msg), 0)) > 0) {

                        vector<string> input_vec;
                        char *message = new char[strlen(recv_msg.buf) + 1];
                        strcpy(message, recv_msg.buf);
                        char *tmp_str = strtok(message, " ");
                        while (tmp_str != NULL) {
                            input_vec.push_back(string(tmp_str));
                            tmp_str = strtok(NULL, " ");
                        }

                        if (strcmp(recv_msg.buf, "exit") == 0) {
                            close(i);
                            FD_CLR(i, &read_fd);
                        }

                        else if (strcmp(recv_msg.buf, "ls") == 0) {
                            bzero(dir_buf, sizeof(char) * BUFF_SIZE);
                            struct dirent *pDirent;
                            DIR *pDir;
                            pDir = opendir("./server_dir");
                            while ((pDirent = readdir(pDir)) != NULL) {
                                if (strcmp(pDirent->d_name, ".") == 0)
                                    continue;
                                if (strcmp(pDirent->d_name, "..") == 0)
                                    continue;
                                sprintf(tmp_buf, "%s ", pDirent->d_name);
                                strcat(dir_buf, tmp_buf);
                            }
                            closedir(pDir);

                            if (strlen(dir_buf) == 0)
                                strcpy(dir_buf, " ");
                            bzero(send_msg.buf, sizeof(char) * BUFF_SIZE);
                            strcpy(send_msg.buf, dir_buf);
                            if ((sent = send(i, &send_msg, sizeof(Msg), 0)) <= 0) {
                                printf("Client disconnected\n");
                                continue;
                            }
                        }

                        else if ((strcmp(input_vec[0].c_str(), "put") == 0) && (input_vec.size() == 2)) {
                            bzero(recv_msg.buf, sizeof(char) * BUFF_SIZE);
                            if ((recved = recv(i, &recv_msg, sizeof(Msg), 0)) > 0) {
                                if (strcmp(recv_msg.buf, "file exists") == 0) {
                                    bzero(filename, sizeof(char) * BUFF_SIZE);
                                    strcpy(filename, "./server_dir/");
                                    strcat(filename, input_vec[1].c_str());

                                    FILE *fp = fopen(filename, "wb");
                                    bzero(recv_msg.buf, sizeof(char) * BUFF_SIZE);
                                    int nbytes, sum = 0;
                                    while ((nbytes = recv(i, &recv_msg, sizeof(Msg), 0)) > 0) {
                                        if (recv_msg.flag == 1)
                                            break;
                                        sum += recv_msg.nbytes;
                                        printf("sum = %d\n", sum);
                                        fwrite(recv_msg.buf, sizeof(char), recv_msg.nbytes, fp);
                                        bzero(recv_msg.buf, sizeof(char) * BUFF_SIZE);
                                    }
                                    fclose(fp);

                                    bzero(send_msg.buf, sizeof(char) * BUFF_SIZE);
                                    sprintf(send_msg.buf, "The %s is uploaded.", input_vec[1].c_str());
                                    if ((sent = send(i, &send_msg, sizeof(Msg), 0)) <= 0) {
                                        printf("Client disconnected\n");
                                        continue;
                                    }
                                }
                                else {
                                    bzero(send_msg.buf, sizeof(char) * BUFF_SIZE);
                                    sprintf(send_msg.buf, "The %s doesn't exist.", input_vec[1].c_str());
                                    if ((sent = send(i, &send_msg, sizeof(Msg), 0)) <= 0) {
                                        printf("Client disconnected\n");
                                        continue;
                                    }
                                }
                            }
                        }

                        else if (strcmp(input_vec[0].c_str(), "get") == 0) {
                            if (input_vec.size() == 2) {
                                int flag = 0;
                                struct dirent *pDirent;
                                DIR *pDir;
                                pDir = opendir("./server_dir");
                                while ((pDirent = readdir(pDir)) != NULL) {
                                    if (strcmp(pDirent->d_name, input_vec[1].c_str()) == 0) {
                                        flag = 1;
                                        break;
                                    }
                                }
                                closedir(pDir);

                                if (flag == 1) {
                                    bzero(send_msg.buf, sizeof(char) * BUFF_SIZE);
                                    strcpy(send_msg.buf, "file exists");
                                    if ((sent = send(i, &send_msg, sizeof(Msg), 0)) <= 0) {
                                        printf("Client disconnected\n");
                                        continue;
                                    }

                                    bzero(filename, sizeof(char) * BUFF_SIZE);
                                    strcpy(filename, "./server_dir/");
                                    strcat(filename, input_vec[1].c_str());

                                    FILE *fp = fopen(filename, "rb");
                                    bzero(send_msg.buf, sizeof(char) * BUFF_SIZE);
                                    int nbytes, sum, cnt = 0;
                                    while ((nbytes = fread(send_msg.buf, sizeof(char), BUFF_SIZE, fp)) > 0)
                                    {
                                        send_msg.nbytes = nbytes;
                                        if ((sent = send(i, &send_msg, sizeof(Msg), 0)) <= 0) {
                                            cnt = 1;
                                            break;
                                        }
                                        bzero(send_msg.buf, sizeof(char) * BUFF_SIZE);
                                    }
                                    fclose(fp);
                                    if (cnt == 1) {
                                        printf("Client disconnected\n");
                                        close(i);
                                        FD_CLR(i, &read_fd);
                                        continue;
                                    }

                                    send_msg.flag = 1;
                                    bzero(send_msg.buf, sizeof(char) * BUFF_SIZE);
                                    if ((sent = send(i, &send_msg, sizeof(Msg), 0)) <= 0) {
                                        printf("Client disconnected\n");
                                        continue;
                                    }
                                    sprintf(send_msg.buf, "The %s is downloaded.", input_vec[1].c_str());
                                    if ((sent = send(i, &send_msg, sizeof(Msg), 0)) <= 0) {
                                        printf("Client disconnected\n");
                                        continue;
                                    }
                                }
                                else
                                {
                                    bzero(send_msg.buf, sizeof(char) * BUFF_SIZE);
                                    sprintf(send_msg.buf, "The %s doesn't exist.", input_vec[1].c_str());
                                    if ((sent = send(i, &send_msg, sizeof(Msg), 0)) <= 0) {
                                        printf("Client disconnected\n");
                                        continue;
                                    }
                                    if ((sent = send(i, &send_msg, sizeof(Msg), 0)) <= 0) {
                                        printf("Client disconnected\n");
                                        continue;
                                    }
                                }
                            }

                            else {
                                bzero(send_msg.buf, sizeof(char) * BUFF_SIZE);
                                strcpy(send_msg.buf, "Command format error.");
                                if ((sent = send(i, &send_msg, sizeof(Msg), 0)) <= 0) {
                                    printf("Client disconnected\n");
                                    continue;
                                }
                            }
                        }

                    
                        else if (strcmp(input_vec[0].c_str(), "play") == 0) {
                            if (input_vec.size() == 2) {
                                int flag = 0;
                                struct dirent *pDirent;
                                DIR *pDir;
                                pDir = opendir("./server_dir");
                                while ((pDirent = readdir(pDir)) != NULL) {
                                    if (strcmp(pDirent->d_name, input_vec[1].c_str()) == 0) {
                                        int length = strlen(input_vec[1].c_str());
                                        char type[4] = {};
                                        bzero(type, sizeof(char) * 4);
                                        strncpy(type, input_vec[1].c_str()+length-3, 3);
                                        if (strcmp("mpg", type) == 0)
                                            flag = 1;
                                        else 
                                            flag = -1;
                                        break;
                                    }
                                }
                                closedir(pDir);

                                if (flag == 1) {
                                    bzero(filename, sizeof(char) * BUFF_SIZE);
                                    strcpy(filename, "./server_dir/");
                                    strcat(filename, input_vec[1].c_str());

                                    VideoCapture cap(filename);
                                    int width = cap.get(CV_CAP_PROP_FRAME_WIDTH);
                                    int height = cap.get(CV_CAP_PROP_FRAME_HEIGHT);

                                    bzero(send_msg.buf, sizeof(char) * BUFF_SIZE);
                                    send_msg.flag = width;
                                    send_msg.nbytes = height;
                                    strcpy(send_msg.buf, "file exists");
                                    if ((sent = send(i, &send_msg, sizeof(Msg), 0)) <= 0) {
                                        printf("Client disconnected\n");
                                        continue;
                                    }

                                    Mat imgServer;
                                    imgServer = Mat::zeros(height, width, CV_8UC3);
                                    int imgSize = imgServer.total() * imgServer.elemSize();

                                    if (!imgServer.isContinuous()) {
                                        imgServer = imgServer.clone();
                                    }

                                    int cnt = 0;
                                    send_msg.flag = 0;
                                    bzero(send_msg.buf, sizeof(char) * BUFF_SIZE);
                                    while (1) {
                                        cap >> imgServer;
                                        if (imgServer.empty()) {
                                            send_msg.flag = 1;
                                            if ((sent = send(i, &send_msg, sizeof(Msg), 0)) <= 0) {
                                                cnt = 1;
                                            }
                                            break;
                                        }

                                        if ((recved = recv(i, &recv_msg, sizeof(Msg), MSG_DONTWAIT)) > 0) {
                                            send_msg.flag = 1;
                                            if ((sent = send(i, &send_msg, sizeof(Msg), 0)) <= 0) {
                                                cnt = 1;
                                            }
                                            break;
                                        }
                                        else {
                                            if ((sent = send(i, &send_msg, sizeof(Msg), 0)) <= 0) {
                                                cnt = 1;
                                                break;
                                            }
                                        }
                                        if ((sent = send(i, imgServer.data, imgSize, 0)) <= 0) {
                                            cnt = 1;
                                            break;
                                        }
                                    }
                                    cap.release();
                                    if (cnt == 1) {
                                        printf("Client disconnected\n");
                                        close(i);
                                        FD_CLR(i, &read_fd);
                                        continue;
                                    }

                                    bzero(send_msg.buf, sizeof(char) * BUFF_SIZE);
                                    strcpy(send_msg.buf, "Finish playing the video.");
                                    if ((sent = send(i, &send_msg, sizeof(Msg), 0)) <= 0) {
                                        printf("Client disconnected\n");
                                        continue;
                                    }
                                }
                                else if (flag == -1) {
                                    bzero(send_msg.buf, sizeof(char) * BUFF_SIZE);
                                    sprintf(send_msg.buf, "The %s is not a mpg file.", input_vec[1].c_str());
                                    if ((sent = send(i, &send_msg, sizeof(Msg), 0)) <= 0) {
                                        printf("Client disconnected\n");
                                        continue;
                                    }
                                    if ((sent = send(i, &send_msg, sizeof(Msg), 0)) <= 0) {
                                        printf("Client disconnected\n");
                                        continue;
                                    }
                                }
                                else {
                                    bzero(send_msg.buf, sizeof(char) * BUFF_SIZE);
                                    sprintf(send_msg.buf, "The %s doesn't exist.", input_vec[1].c_str());
                                    if ((sent = send(i, &send_msg, sizeof(Msg), 0)) <= 0) {
                                        printf("Client disconnected\n");
                                        continue;
                                    }
                                    if ((sent = send(i, &send_msg, sizeof(Msg), 0)) <= 0) {
                                        printf("Client disconnected\n");
                                        continue;
                                    }
                                }
                            }

                            else {
                                bzero(send_msg.buf, sizeof(char) * BUFF_SIZE);
                                strcpy(send_msg.buf, "Command format error.");
                                if ((sent = send(i, &send_msg, sizeof(Msg), 0)) <= 0) {
                                    printf("Client disconnected\n");
                                    continue;
                                }
                            }
                        }

                        else {
                            bzero(send_msg.buf, sizeof(char) * BUFF_SIZE);
                            strcpy(send_msg.buf, "Command not found.");
                            if ((sent = send(i, &send_msg, sizeof(Msg), 0)) <= 0) {
                                printf("Client disconnected\n");
                                continue;
                            }
                        }
                    }
                }
            }
        }
    }
    return 0;
}


