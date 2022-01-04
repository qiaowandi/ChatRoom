#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <thread>

using std::perror;

#define BUFFER_LEN 1024
#define NAME_LEN 1024
#define MAX_CLIENT_NUM 32
#define SERVER_PORT 8888

struct Client
{
    int valid;               //to judge whether this user is online
    int fd_id;               //user ID number
    int socket;              //socket to this user
    char name[NAME_LEN + 1]; //name of the user
}
//client array
client[MAX_CLIENT_NUM] = {0};
//mutex
std::mutex num_mutex;
//2 kind of Threads
std::thread chat_thread[MAX_CLIENT_NUM];
std::thread send_thread[MAX_CLIENT_NUM];
//used for sync message
std::mutex client_mutex[MAX_CLIENT_NUM];
std::condition_variable cv[MAX_CLIENT_NUM];
std::queue<std::string> message_q[MAX_CLIENT_NUM];
int curClientNum = 0;

void *handle_send(void *data)
{
    struct Client *pipe = (struct Client *)data;
    while (1)
    {
        //wait until new message receive
        while (message_q[pipe->fd_id].empty())
        {
            std::unique_lock<std::mutex> mLock(client_mutex[pipe->fd_id]);
            cv[pipe->fd_id].wait(mLock);
        }
        //if message queue isn't full, send message
        while (!message_q[pipe->fd_id].empty())
        {
            //get the first message from the queue
            std::string message_buffer = message_q[pipe->fd_id].front();
            int n = message_buffer.length();
            //calculate one transfer length
            int trans_len = BUFFER_LEN > n ? n : BUFFER_LEN;
            //send the message
            while (n > 0)
            {
                int len = send(pipe->socket, message_buffer.c_str(), trans_len, 0);
                if (len < 0)
                {
                    perror("send");
                    return NULL;
                }
                n -= len;
                message_buffer.erase(0, len); //delete data that has been transported
                trans_len = BUFFER_LEN > n ? n : BUFFER_LEN;
            }
            //delete the message that has been sent
            message_buffer.clear();
            message_q[pipe->fd_id].pop();
        }
    }
    return NULL;
}

//get client message and push into queue
void handle_recv(void *data)
{
    struct Client *pipe = (struct Client *)data;
    // message buffer
    std::string message_buffer;
    int message_len = 0;
    // one transfer buffer
    char buffer[BUFFER_LEN + 1];
    int buffer_len = 0;
    // receive
    while ((buffer_len = recv(pipe->socket, buffer, BUFFER_LEN, 0)) > 0)
    {
        //to find '\n' as the end of the message
        for (int i = 0; i < buffer_len; i++)
        {
            //the start of a new message
            if (message_len == 0)
            {
                char temp[100];
                sprintf(temp, "%s:", pipe->name);
                message_buffer = temp;
                message_len = message_buffer.length();
            }
            message_buffer += buffer[i];
            message_len++;
            if (buffer[i] == '\n')
            {
                //send to every client
                for (int j = 0; j < MAX_CLIENT_NUM; j++)
                {
                    if (client[j].valid && client[j].socket != pipe->socket)
                    {
                        message_q[j].push(message_buffer);
                        cv[j].notify_one();
                    }
                }
                //new message start
                message_len = 0;
                message_buffer.clear();
            }
        }
        //clear buffer
        buffer_len = 0;
        memset(buffer, 0, sizeof(buffer));
    }
    return;
}

//debug1
void chat(void* data)
{
    struct Client* user = (struct Client*)data;
    char message[100];
    sprintf(message, "Hello %s, Welcome to join the chat room. Online User Number: %d\n", user->name, curClientNum);
    message_q[user->fd_id].push(message);
    cv[user->fd_id].notify_one();

    memset(message, 0, sizeof(message));
    sprintf(message, "New User %s join in! Online User Number: %d\n", user->name, curClientNum);
    //send messages to other users
    for (int j = 0; j < MAX_CLIENT_NUM; j++)
    {
        if (client[j].valid && client[j].socket != user->socket)
        {
            message_q[j].push(message);
            cv[j].notify_one();
        }
    }
    send_thread[user->fd_id] = std::thread(handle_send, (void *)user);
    send_thread[user->fd_id].detach();

    //receive message
    //Block
    handle_recv(data);

    num_mutex.lock();
    user->valid = 0;
    curClientNum--;
    num_mutex.unlock();
    //printf bye message
    printf("%s left the chat room. Online Person Number: %d\n", user->name, curClientNum);
    char bye[100];
    sprintf(bye, "%s left the chat room. Online Person Number: %d\n", user->name, curClientNum);
    //send offline message to other clients
    for (int j = 0; j < MAX_CLIENT_NUM; j++)
    {
        if (client[j].valid && client[j].socket != user->socket)
        {
            message_q[j].push(bye);
            cv[j].notify_one();
        }
    }

}
int main()
{
    //Create socket
    //socket(Ipaddr,Socket type, protocol)
    //socket(IPv4, SOCK_STREAM, TCP/UDP)
    //if create error return socket return -1
    int serverSock = socket(AF_INET, SOCK_STREAM, 0);
    if(serverSock == -1)
    {
        perror("Create Socket Error");
        return -1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(SERVER_PORT); //127.0.0.1:8888

    if(bind(serverSock, (struct sockaddr*)&addr, sizeof(addr)) == -1)
    {
        perror("bind error");
        return -1;
    }
    //no more than 32 clients
    //serverSock used for listen
    if (listen(serverSock, MAX_CLIENT_NUM + 1) < 0)
    {
        perror("listen");
        return -1;
    }
    
    printf("Server start successfully!\n");
    printf("You can join the chat room by connecting to 127.0.0.1:8888\n\n");
    //Start Event Loop
    while(1)
    {
        //create a new socket for new connect
        int conn = accept(serverSock, NULL, NULL);

        if (conn == -1)
        {
            perror("accept");
            return -1;
        }

        if (curClientNum >= MAX_CLIENT_NUM)
        {
            if (send(conn, "ERROR", strlen("ERROR"), 0) < 0)
                perror("send");
            shutdown(conn, 2);
            continue;
        }
        else
        {
            if (send(conn, "OK", strlen("OK"), 0) < 0)
                perror("send");
        }

        char name[NAME_LEN + 1] = {0};

        //recv user name from client
        ssize_t user = recv(conn, name,NAME_LEN, 0);
        if (user < 0)
        {
            perror("Recv error");
            //shutdown conn read&write function
            shutdown(conn, 2);
            continue;
        }
        else if(user == 0)
        {
            shutdown(conn, 2);
            continue;;
        }

        //update client array
        for(int i = 0; i < MAX_CLIENT_NUM; i++)
        {
            //whether client[i] has been used
            if(!client[i].valid)
            {
                num_mutex.lock();
                memset(client[i].name, 0, sizeof(client[i].name));
                strcpy(client[i].name, name);

                client[i].valid = 1;
                client[i].fd_id = i;
                client[i].socket = conn;

                curClientNum++;
                num_mutex.unlock();
                chat_thread[i] = std::thread(chat, (void *)&client[i]);
                chat_thread[i].detach();

                printf("%s join in the chat room. Online User Number: %d\n", client[i].name, curClientNum);
                break;
            }
        }
    }
    for(int i = 0; i < MAX_CLIENT_NUM; i++)
    {
        if(client[i].valid)
        {
            shutdown(client->socket, 2);
        }
    }
    shutdown(serverSock, 2);
    return 0;
}