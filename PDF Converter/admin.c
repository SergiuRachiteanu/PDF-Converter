#include <arpa/inet.h>
#include <libgen.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/sendfile.h>
#include <unistd.h>

#include <pthread.h>
#include <semaphore.h>

typedef struct sockaddr sockaddr_t;


#define BUFFER_SIZE 1024         // 1024 KB = 1 MB


int open_client_fd(char *hostname, int port)
{
    int client_fd;
    struct hostent *hp;
    struct sockaddr_in server_addr;
    
    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1; 
    
    // Fill in the server's IP address and port 
    if ((hp = gethostbyname(hostname)) == NULL)
        return -2; // check h_errno for cause of error 
    
    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    bcopy((char *) hp->h_addr, 
          (char *) &server_addr.sin_addr.s_addr, hp->h_length);
    server_addr.sin_port = htons(port);
    
    // Establish a connection with the server 
    if (connect(client_fd, (sockaddr_t *) &server_addr, sizeof(server_addr)) < 0)
        return -1;
    
    return client_fd;
}

int print_menu_get_option ()
{
    int input_option = -1;
    char eol_char;
    
    printf("\nEnter the file type to send.\n");
    printf("    0. LOG-OUT\n");
    printf("    1. INSERT USER\n");
    printf("    2. DELETE USER\n");
    printf("    3. MODIFY USER\n");
    printf("\nEnter Option (0 - 3): ");
    
    scanf("%d%c", &input_option, &eol_char);
    
    return input_option;
}

int handle_admin_operations(int client_fd)
{
    // create a send / recv buffer
    char send_buffer[BUFFER_SIZE], recv_buffer[BUFFER_SIZE];
    
    // loop until done
    while(1)
    {
        // print sub-menu to get admins desired task
        int option = print_menu_get_option();
        
        if ((option < 0) || (option > 3))
        {
            // print error message
            fprintf(stderr, "Invalid User Option --> %d\n", option);   
            
            // get user option again
            continue;
        }
        
        // send selected sub-option to server
        sprintf(send_buffer, "%d", option);
        
        // write the option to the server side
        write(client_fd, send_buffer, BUFFER_SIZE);

        // process user-provided option
        if (option == 0)
        {
            // exit was requested
            break;
        }
        else if (option == 1)
        {
            // re-initialize the buffer
            memset(send_buffer, '\0', BUFFER_SIZE);
            
            // ask the client for their username and send to server
            printf("\nENTER DESIRED USERNAME: ");
            fgets(send_buffer, sizeof(send_buffer), stdin);
            send_buffer[strlen(send_buffer)-1] = '\0';
            write(client_fd, send_buffer, BUFFER_SIZE);
            
            // re-initialize the buffer
            memset(send_buffer, '\0', BUFFER_SIZE);
            
            // ask the client for their password and send to server
            printf("ENTER DESIRED PASSWORD: ");
            fgets(send_buffer, sizeof(send_buffer), stdin);
            send_buffer[strlen(send_buffer)-1] = '\0';
            write(client_fd, send_buffer, BUFFER_SIZE);

            // re-initialize the buffer
            memset(send_buffer, '\0', BUFFER_SIZE);
            
            int user_role;
            char eol_char;
            
            while (1)
            {
                // ask the client for their password and send to server
                printf("ENTER DESIRED USER-ROLE [0-USER, 1-ADMIN] (0 - 1): ");
                scanf("%d%c", &user_role, &eol_char);
                
                if ((user_role != 0) && (user_role != 1))
                {
                    fprintf(stdout, "Invalid Option selected! Try Again!\n");
                    continue;
                }
                else
                {
                    sprintf(send_buffer, "%d", user_role);
                    write(client_fd, send_buffer, BUFFER_SIZE);
                    break;
                }
            }
            
            // re-initialize the buffer
            memset(recv_buffer, '\0', BUFFER_SIZE);
            
            // get response from the server (F, U)
            recv(client_fd, recv_buffer, BUFFER_SIZE, 0);
            
            // check if the return status was success or error message
            if (strcmp(recv_buffer, "SUCCESS") != 0)
            {
                // some error occured on server side
                // print the message sent by server side
                printf("SERVER_RESPONSE - %s", recv_buffer);
                
                // since there was an error it is time to exit
                break;
            }
            else
            {
                fprintf(stdout, "\nNEW USER ADDED SUCCESSFULLY\n");
            }
            
        }
        else if (option == 2)
        {
            // re-initialize the buffer
            memset(send_buffer, '\0', BUFFER_SIZE);
            
            // ask the client for their username and send to server
            printf("\nENTER USERNAME TO DELETE: ");
            fgets(send_buffer, sizeof(send_buffer), stdin);
            send_buffer[strlen(send_buffer)-1] = '\0';
            write(client_fd, send_buffer, BUFFER_SIZE);
            
            // re-initialize the buffer
            memset(recv_buffer, '\0', BUFFER_SIZE);
            
            // get response from the server (F, U)
            recv(client_fd, recv_buffer, BUFFER_SIZE, 0);
            
            // check if the return status was success or error message
            if (strcmp(recv_buffer, "SUCCESS") != 0)
            {
                // some error occured on server side
                // print the message sent by server side
                printf("SERVER_RESPONSE - %s", recv_buffer);
                
                // since there was an error it is time to exit
                break;
            }
            else
            {
                fprintf(stdout, "\nUSER REMOVED SUCCESSFULLY\n");
            }
        }
        else if (option == 3)
        {
            // re-initialize the buffer
            memset(send_buffer, '\0', BUFFER_SIZE);
            
            // ask the client for their username and send to server
            printf("\nENTER USERNAME TO EDIT: ");
            fgets(send_buffer, sizeof(send_buffer), stdin);
            send_buffer[strlen(send_buffer)-1] = '\0';
            write(client_fd, send_buffer, BUFFER_SIZE);
            
            // re-initialize the buffer
            memset(send_buffer, '\0', BUFFER_SIZE);
            
            // ask the client for their username and send to server
            printf("\nENTER NEW USERNAME: ");
            fgets(send_buffer, sizeof(send_buffer), stdin);
            send_buffer[strlen(send_buffer)-1] = '\0';
            write(client_fd, send_buffer, BUFFER_SIZE);
            
            // re-initialize the buffer
            memset(send_buffer, '\0', BUFFER_SIZE);
            
            // ask the client for their password and send to server
            printf("ENTER NEW PASSWORD: ");
            fgets(send_buffer, sizeof(send_buffer), stdin);
            send_buffer[strlen(send_buffer)-1] = '\0';
            write(client_fd, send_buffer, BUFFER_SIZE);

            // re-initialize the buffer
            memset(send_buffer, '\0', BUFFER_SIZE);
            
            int user_role;
            char eol_char;
            
            while (1)
            {
                // ask the client for their password and send to server
                printf("ENTER NEW DESIRED USER-ROLE [0-USER, 1-ADMIN] (0 - 1): ");
                scanf("%d%c", &user_role, &eol_char);
                
                if ((user_role != 0) && (user_role != 1))
                {
                    fprintf(stdout, "Invalid Option selected! Try Again!\n");
                    continue;
                }
                else
                {
                    sprintf(send_buffer, "%d", user_role);
                    write(client_fd, send_buffer, BUFFER_SIZE);
                    break;
                }
            }
            
            // re-initialize the buffer
            memset(recv_buffer, '\0', BUFFER_SIZE);
            
            // get response from the server (F, U)
            recv(client_fd, recv_buffer, BUFFER_SIZE, 0);
            
            // check if the return status was success or error message
            if (strcmp(recv_buffer, "SUCCESS") != 0)
            {
                // some error occured on server side
                // print the message sent by server side
                printf("SERVER_RESPONSE - %s", recv_buffer);
                
                // since there was an error it is time to exit
                break;
            }
            else
            {
                fprintf(stdout, "\nUSER MODIFIED SUCCESSFULLY\n");
            }
        }
        
    }
    return 0;
}

int main (int argc, char **argv)
{
    int c, return_status = 0;
    
    int port = 8990;
    char host[50] = "127.0.0.1";
    
    // read / process arguments provided by the user
    while ((c = getopt(argc, argv, "p:h:")) != -1)
    {
        switch (c) 
        {
            case 'p':
                port = atoi(optarg);
                break;
            case 'h':
                sprintf(host, "%s", optarg);
                break;
            default:
                fprintf(stderr, "usage: ./server [-h <host_address>] [-p <portnum>] \n");
                exit(1);
        }
    }
    
    // connect to the remote server
    int client_fd = open_client_fd(host, port);
    
    // create a send / recv buffer
    char send_buffer[BUFFER_SIZE], recv_buffer[BUFFER_SIZE];
    
    // loop forever to accept client connections
    while (1)
    {
        // send option 2 to tell server this is admin client
        sprintf(send_buffer, "%d", 2);
        
        // write the option to the server side
        write(client_fd, send_buffer, BUFFER_SIZE);
        
        // get the credentials from the user and send
        // them to server for verification
        
        // re-initialize the buffer
        memset(send_buffer, '\0', BUFFER_SIZE);
            
        // ask the client for their username and send to server
        printf("\nENTER USERNAME: ");
        fgets(send_buffer, sizeof(send_buffer), stdin);
        send_buffer[strlen(send_buffer)-1] = '\0';
        write(client_fd, send_buffer, BUFFER_SIZE);
        
        // re-initialize the buffer
        memset(send_buffer, '\0', BUFFER_SIZE);
        
        // ask the client for their password and send to server
        printf("ENTER PASSWORD: ");
        fgets(send_buffer, sizeof(send_buffer), stdin);
        send_buffer[strlen(send_buffer)-1] = '\0';
        write(client_fd, send_buffer, BUFFER_SIZE);

        // re-initialize the buffer
        memset(recv_buffer, '\0', BUFFER_SIZE);
        
        // get response from the server (F, U)
        recv(client_fd, recv_buffer, BUFFER_SIZE, 0);
        
        // check if the return status was success or error message
        if (strcmp(recv_buffer, "SUCCESS") != 0)
        {
            // some error occured on server side
            // print the message sent by server side
            printf("SERVER_RESPONSE - %s", recv_buffer);
            
            // since there was an error it is time to exit
            break;
        }
        else
        {
            handle_admin_operations(client_fd);
        }
    }
    
    // close the socket connection
    close(client_fd);
    
    // successful exit (will never happen)
    return return_status;
}