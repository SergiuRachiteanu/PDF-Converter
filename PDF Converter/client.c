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

int print_main_menu_get_option( )
{
    int input_option = -1;
    char eol_char;
    
    printf("\nEnter the desired option #\n");
    printf("    0. LOGIN\n");
    printf("    1. REGISTER\n");
    printf("    2. EXIT\n\n");
    printf("Enter Option (0 - 2): ");
    
    scanf("%d%c", &input_option, &eol_char);
    
    return input_option;
}

int print_submenu_get_option ()
{
    int input_option = -1;
    char eol_char;
    
    printf("\nEnter the file type to send.\n");
    printf("    0. LOG-OUT\n");
    printf("    1. TEXT FILE (.txt)\n");
    printf("    2. WORD FILE (.docx)\n");
    printf("    3. Image FILE (.png, .jpg)\n\n");
    printf("Enter Option (0 - 3): ");
    
    scanf("%d%c", &input_option, &eol_char);
    
    return input_option;
}


int handle_file_send(int client_fd)
{
    // create a send buffer
    char send_buffer[BUFFER_SIZE], recv_buffer[BUFFER_SIZE], temp_buffer[BUFFER_SIZE];
    int return_status = 0;
    
    while (1)
    {
        // print the sub-menu and get file type to send
        int opt = print_submenu_get_option();
        
        // check for invalid sub-menu option
        if (opt < 0 || opt > 3)
        {
            // print error message and update exit status
            fprintf(stderr, "Invalid User Option --> %d\n", opt);   
            
            // continue loop until user enters correct option
            continue;
        }
        
        // put the selected option into a buffer
        sprintf(send_buffer, "%d", opt);
        
        // write the option to the server side
        write(client_fd, send_buffer, strlen(send_buffer));
        
        // exit if option for logout is selected
        if (opt == 0)
        {
            // update exit status
            return_status = 1;
            
            // exit the loop
            break;
        }
        
        // re-initialize the buffer
        memset(send_buffer, '\0', BUFFER_SIZE);
        
        // ask the client for file path and then handle it
        printf("\n\nEnter Filepath + Filename: ");
        fgets(temp_buffer, sizeof(temp_buffer), stdin);
        temp_buffer[strlen(temp_buffer)-1] = '\0';
        
        // open the desired file
        int send_file_fd = open(temp_buffer, O_RDONLY);
        if (send_file_fd == -1)
        {
            // send error status to server
            sprintf(send_buffer, "%d", -1);
            write(client_fd, send_buffer, BUFFER_SIZE);
            
            // update exit status
            return_status = -2;
            
            // print out a console message and exit
            fprintf(stderr, "Error opening file --> %s", strerror(errno));
            break;
        }
        
        struct stat file_stat;
        
        // get file stats
        if (fstat(send_file_fd, &file_stat) < 0)
        {
            // close the file descriptor
            close(send_file_fd);

            // send error status to server
            sprintf(send_buffer, "%d", -1);
            write(client_fd, send_buffer, BUFFER_SIZE);
            
            // update exit status
            return_status = -2;
            
            // print out a console message and exit
            fprintf(stderr, "Error fstat --> %s", strerror(errno));
            break;
        }
        
        // send the file size to server
        sprintf(send_buffer, "%ld", file_stat.st_size);
        write(client_fd, send_buffer, BUFFER_SIZE);
        
        // write the actual file to the server side
        off_t offset = 0;
        int sent_bytes = 0;
        int remain_data = file_stat.st_size;
        
        // sending file data
        while (((sent_bytes = sendfile(client_fd, send_file_fd, &offset, BUFFER_SIZE)) > 0) && (remain_data > 0))
        {
                fprintf(stdout, "1. Client sent %d bytes from file's data, offset is now : %ld and remaining data = %d\n", sent_bytes, offset, remain_data);
                remain_data -= sent_bytes;
                fprintf(stdout, "2. Client sent %d bytes from file's data, offset is now : %ld and remaining data = %d\n", sent_bytes, offset, remain_data);
        }
        
        // close the file
        close(send_file_fd);
        




        /***********************************************************
        ************************************************************
        ****                                                    ****
        ****                                                    ****
        ****    Successfully sent to server now wait for        ****
        ****    Server to convert file to pdf and send back     ****
        ****                                                    ****
        ****                                                    ****
        ************************************************************
        ************************************************************/



        
        // re-initialize the buffer
        memset(recv_buffer, '\0', BUFFER_SIZE);
        
        // receive the file size
        recv(client_fd, recv_buffer, BUFFER_SIZE, 0);
        int file_size = atoi(recv_buffer);
        
        // verify correct filesize was sent
        if (file_size < 0)
        {
            // update exit status
            return_status = -3;
            
            // print out a console message and exit
            fprintf(stderr, "Error invalid file size");
            break;
        }
        
        // setup the paths variables
        char *fname_only = basename(temp_buffer);
        
        // remove the extension of the file based on
        // selected file type (.txt, .docx, .pgn, .jpg, .jpeg)
        for (int i = strlen(fname_only)-1; i >= 0; i--)
        {
            // starting from right side if dot (.) is found
            // then replace that with a NULL character.
            if (fname_only[i] == '.')
            {
                fname_only[i] = '\0';
                break;
            }
        }
        /*
        if (opt == 1)
        {
            // .txt file remove last 4 chars to get filename only
            fname_only[strlen(fname_only)-4] = '\0';
        }
        else if (opt == 2)
        {
            // .docs file remove last 5 chars to get filename only
            fname_only[strlen(fname_only)-5] = '\0';
        }
        */
        
        char temp_pf_full[200];
        
        sprintf(temp_pf_full, "%s.pdf", fname_only);
        
        // open the file to write
        FILE *received_file;
        received_file = fopen(temp_pf_full, "w");
        
        // check for errors
        if (received_file == NULL)
        {
            // update exit status
            return_status = -4;
            
            // print out a console message and exit
            fprintf(stderr, "Failed to open file foo --> %s\n", strerror(errno));
            return -2;
        }
        
        // start receiving the file from client
        remain_data = file_size;
        int len = 0;
        while ((remain_data > 0) && ((len = recv(client_fd, recv_buffer, BUFFER_SIZE, 0)) > 0))
        {
                fwrite(recv_buffer, sizeof(char), len, received_file);
                remain_data -= len;
                fprintf(stdout, "Receive %d bytes and we hope :- %d bytes\n", len, remain_data);
        }
        
        // close the file
        fclose(received_file);
        
        // we are done let's end the client
        break;
    }
    
    return return_status;
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
    
    // create a send buffer
    char send_buffer[BUFFER_SIZE], recv_buffer[BUFFER_SIZE];
    
    // loop forever to accept client connections
    while (1)
    {
        // print the main menu and get desired option
        int opt = print_main_menu_get_option();
        
        if (opt < 0 || opt > 2)
        {
            // print error message and update exit status
            fprintf(stderr, "Invalid User Option --> %d\n", opt);   
            
            // exit the loop
            continue;
        }
        
        // put the selected option into a buffer
        sprintf(send_buffer, "%d", (opt==2)? 10 : opt);
        
        // write the option to the server side
        write(client_fd, send_buffer, strlen(send_buffer));
        
        // based on the selected option perform
        // desired task i.e. LOGIN or REGISTER or EXIT
        if (opt == 0)
        {
            // re-initialize the buffer
            memset(send_buffer, '\0', BUFFER_SIZE);
            
            // ask the client for their username and send to server
            printf("\n\nENTER YOUR USERNAME: ");
            fgets(send_buffer, sizeof(send_buffer), stdin);
            send_buffer[strlen(send_buffer)-1] = '\0';
            write(client_fd, send_buffer, strlen(send_buffer));
            
            // re-initialize the buffer
            memset(send_buffer, '\0', BUFFER_SIZE);
            
            // ask the client for their password and send to server
            printf("ENTER YOUR PASSWORD: ");
            fgets(send_buffer, sizeof(send_buffer), stdin);
            send_buffer[strlen(send_buffer)-1] = '\0';
            write(client_fd, send_buffer, strlen(send_buffer));

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
                if (handle_file_send(client_fd) == 1)
                    continue;
                else
                    break;
            }
        }
        else if (opt == 1)
        {
            // re-initialize the buffer
            memset(send_buffer, '\0', BUFFER_SIZE);
            
            // ask the client for their username and send to server
            printf("\n\nENTER DESIRED USERNAME: ");
            fgets(send_buffer, sizeof(send_buffer), stdin);
            send_buffer[strlen(send_buffer)-1] = '\0';
            write(client_fd, send_buffer, strlen(send_buffer));
            
            // re-initialize the buffer
            memset(send_buffer, '\0', BUFFER_SIZE);
            
            // ask the client for their password and send to server
            printf("ENTER DESIRED PASSWORD: ");
            fgets(send_buffer, sizeof(send_buffer), stdin);
            send_buffer[strlen(send_buffer)-1] = '\0';
            write(client_fd, send_buffer, strlen(send_buffer));

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
                // print success message
                fprintf(stdout, "New user added successfully.\n");
            }
                
        }
        else if (opt == 2)
        {
            // option 2: meaning exit from the program
            break;
        }
    }
    
    // close the socket connection
    close(client_fd);
    
    // successful exit (will never happen)
    return return_status;
}