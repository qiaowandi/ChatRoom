#include <iostream>
#include <stdio.h>
#include <string.h>
#include <limits>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <string>
using namespace std;

#define BUFFER_LEN 1024
#define NAME_LEN 20
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8888

char name[NAME_LEN + 1]; //user's name

//receive message and print out
void *handle_recv(void *data)
{
    int pipe = *(int *)data;

    //message buffer
    string message_buffer;
    int message_len = 0;

    //one transfer buffer
    char buffer[BUFFER_LEN + 1];
    int buffer_len = 0;

    //receive
    while((buffer_len = recv(pipe, buffer, BUFFER_LEN, 0)) > 0)
    {
        for(int i = 0; i < buffer_len; i++)
        {
            if(message_len == 0)
                message_buffer = buffer[i];
            else
                message_buffer += buffer[i];

            message_len++;

            if(buffer[i] == '\n')
            {
                cout << message_buffer << endl;

                //new message start
                message_len = 0;
                message_buffer.clear();
            }
        }
        memset(buffer, 0, sizeof(buffer));
    }
    printf("The Server has been shutdown!\n");
    return NULL;
}

int main()
{
    //create a socket to connect with the server
    int client_sock;
    if((client_sock = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket");
        return -1;
    }
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    addr.sin_port = honts(SERVER_PORT);

    //connect the server
    if(connet(client_sock, (struct sockaddr *)&addr, sizeof(addr))) //0-成功，-1-失败
    {
        perror("connect");
        return -1;
    }

    //check state
    printf("Connecting...");
    fflush(stdout);
    char state[10] = {0};
    if(recv(client_sock, state, sizeof(state), 0) < 0)
    {
        perror("recv");
        return -1;
    }
    if(strcmp(state, "OK"))
    {
        printf("\rThe chatroom is already full!\n");
        return 0;
    }
    else
    {
        printf("\rConnect Successfully!\n");
    }

    //get name
    printf("Welcome to Use Multi-Person Chat room!\n");
    while(1)
    {
        printf("Please enter your name:");
        cin.get(name, NAME_LEN);
        int name_len = strlen(name);
        //no input
        if(cin.eof())
        {
            //reset
            cin.clear();
            clearerr(stdin);
            printf("\nYou need to input at least one world!\n");
            continue;
        }
        //single Enter
        else if(name_len == 0)
        {
            //reset
            cin.clear();
            clearerr(stdin);
            cin.get();
            continue;
            printf("\nYou need to input at least one word!\n");
        }
        //overflow
        else if(name_len > NAME_LEN - 2)
        {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            printf("\nReached the upper limit of the words!\n");
            continue;
        }
        cin.get(); //remove '\n' in stdin
        name[NAME_LEN] = '\0';
        break;
    }
    if(send(client_sock, name, strlen(name), 0) < 0)
    {
        perror("send");
        return -1;
    }

    //create a new thread to handle receive messages
    pthread_t recv_thread;
    //first para:Pointer to thread identifier
    //second para:thread attributes
    //third para: the address of thread running function
    //forth para: run function parameters
    pthread_create(&recv_thread, NULL, handle_recv, (void *)&client_sock);

    //get message and send
    while(1)
    {
        char message[BUFFER_LEN + 1];
        cin.get(message, BUFFER_LEN);
        int n = strlen(message);
        if(cin.eof())
        {
            //reset
            cin.clear();
            clearerr(stdin);
            continue;
        }
        //single Enter
        else if(n == 0)
        {
            //reset
            cin.clear();
            clearerr(stdin);
            continue;
        }
        //overflow
        else if(n > BUFFER_LEN - 2)
        {
            //reset
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            printf("Reached the upper limit of the words!\n");
            continue;
        }
        cin.get(); // remove '\n' in stdin
        message[n] = '\n'; // add '\n'
        message[n+1] = '\0';
        n++;
        //the length of message now is n+1
        printf("\n");
        // the length of message that has been sent
        int sent_len = 0;
        //calculate one transfer length
        int transfer_len = BUFFER_LEN > n ? n : BUFFER_LEN;

        //send the message
        while(n>0)
        {
            int len = send(client_sock, message + sent_len, transfer_len, 0);
            if(len < 0)
            {
                perror("send");
                return -1;
            }
            n -= len;
            sent_len += len;
            transfer_len = BUFFER_LEN > n ? n : BUFFER_LEN;
        }
        //clean the buffer
        memset(message, 0, sizeof(message));
    }

    pthread_cancel(recv_thread_thread);
    shutdown(client_sock, 2);
    return 0;
}