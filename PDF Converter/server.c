#include <arpa/inet.h>
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

#define BUFFER_SIZE 1024         // 1 KB
#define PASSWORD_FILE "passWD.txt"
#define TEMP_PATHNAME "./temp"
#define TEMP_FILENAME_1 "temPFile_%d"


typedef struct sockaddr sockaddr_t;

typedef struct {
    char username[50];
    char password[50];
    char acc_type[10];
} PASSWORDS_ST;


int register_user_account(char *username, char *password, char *accountType)
{
    FILE *passWD_f;
    
    // open the password file to append
    passWD_f = fopen(PASSWORD_FILE, "a");
            
    // check for errors
    if (passWD_f == NULL)
        return -1;
    
    // append the new data
    fprintf(passWD_f, "%s %s %s \n", username, password, accountType);
    
    // close the passwords file
    fclose(passWD_f);
    
    // return succcessful status
    return 1;
}

int handle_admin_account(int client_socket)
{
    char recv_buffer[BUFFER_SIZE], send_buffer[BUFFER_SIZE];
    
    // loop forever and service the admin user
    while (1)
    {
        // print out a console message
        printf("Waiting for admin to send option\n");
        
        // re-initialize the buffer
        memset(recv_buffer, '\0', BUFFER_SIZE);

        // receive the client's desired file type
        recv(client_socket, recv_buffer, BUFFER_SIZE, 0);
        int option = atoi(recv_buffer);
        
        if (option == 0)
        {
            printf("Logout Selected By Admin!\n");
            // Logout has been initiated
            return 0;
        }
        else if (option == 1)
        {
            printf("Insert User Account Selected By Admin!\n");
            char userName_recv[50], userName_read[50];
            char passWord_recv[50], passWord_read[50];
            char acctType_recv[10], acctType_read[10];
            
            int account_type = -1;
            
            // receive new username / password / accountType from the client
            recv(client_socket, recv_buffer, BUFFER_SIZE, 0);
            sprintf(userName_recv, "%.49s", recv_buffer);
            
            recv(client_socket, recv_buffer, BUFFER_SIZE, 0);
            sprintf(passWord_recv, "%.49s", recv_buffer);
            
            recv(client_socket, recv_buffer, BUFFER_SIZE, 0);
            account_type = atoi(recv_buffer);
            sprintf(acctType_recv, "%s", (account_type==0)? "USER" : "ADMIN");
            
            // declare the password file variable
            FILE *passWD_f;
            
            // open the password file to verify
            passWD_f = fopen(PASSWORD_FILE, "r");
            
            // check for errors
            if (passWD_f == NULL)
            {
                // re-initialize the buffer
                memset(send_buffer, '\0', BUFFER_SIZE);
                
                // send the error response to client side
                sprintf(send_buffer, "Error: Server failed to open password file --> %s\n", strerror(errno));
                write(client_socket, send_buffer, BUFFER_SIZE);
                
                // exit
                return -1;
            }
            
            // loop through the passwords file and check if
            // user identity already exists or not
            while (!feof(passWD_f))
            {
                // verify if credentials exist or not
                fscanf(passWD_f, "%s ", userName_read);
                fscanf(passWD_f, "%s ", passWord_read);
                fscanf(passWD_f, "%s ", acctType_read);
                
                // compare sent credentials and read credentials
                if (strcmp(userName_recv, userName_read) == 0)
                {
                    // re-initialize the buffer
                    memset(send_buffer, '\0', BUFFER_SIZE);
                
                    // send the error response to client side
                    sprintf(send_buffer, "Error: User-account already exists\n");
                    write(client_socket, send_buffer, BUFFER_SIZE);
                            
                    // close the file
                    fclose(passWD_f);

                    // time to exit the thread
                    return -2;
                }
            }
                
            // close the file
            fclose(passWD_f);
            
            // register the new user account
            if (register_user_account(userName_recv, passWord_recv, acctType_recv) < 0)
            {
                // re-initialize the buffer
                memset(send_buffer, '\0', BUFFER_SIZE);
                
                // send the error response to client side
                sprintf(send_buffer, "Error: Server failed to open password file --> %s\n", strerror(errno));
                write(client_socket, send_buffer, BUFFER_SIZE);
                
                // exit
                return -3;
            }
            else
            {
                // re-initialize the buffer
                memset(send_buffer, '\0', BUFFER_SIZE);
                
                // send the success response to client side
                sprintf(send_buffer, "SUCCESS");
                write(client_socket, send_buffer, BUFFER_SIZE);
            }
        }
        else if (option == 2)
        {
            printf("Delete User Account Selected By Admin!\n");
            char userName_recv[50];
            
            // re-initialize the buffer
            memset(recv_buffer, '\0', BUFFER_SIZE);

            // receive new username / password / accountType from the client
            recv(client_socket, recv_buffer, BUFFER_SIZE, 0);
            sprintf(userName_recv, "%.49s", recv_buffer);
            
            // declare the password file variable
            FILE *passWD_f;
            
            // open the password file to verify
            passWD_f = fopen(PASSWORD_FILE, "r");
            
            // check for errors
            if (passWD_f == NULL)
            {
                // re-initialize the buffer
                memset(send_buffer, '\0', BUFFER_SIZE);
                
                // send the error response to client side
                sprintf(send_buffer, "Error: Server failed to open password file --> %s\n", strerror(errno));
                write(client_socket, send_buffer, BUFFER_SIZE);
                
                // exit
                return -4;
            }
            
            int status = 0, skip = 0, total_creds = 0;
            int allocated = 100;
            PASSWORDS_ST *credentials = (PASSWORDS_ST *)malloc(sizeof(PASSWORDS_ST) * allocated);
            
            // loop through the passwords file and check if
            // user identity already exists or not
            while (!feof(passWD_f))
            {
                // check if more memory needs to be allocated
                if (allocated == total_creds)
                {
                    // allocate 50 more space
                    allocated += 50;
                    credentials = (PASSWORDS_ST *)realloc(credentials, sizeof(PASSWORDS_ST) * allocated);
                }
                
                // verify if credentials exist or not
                fscanf(passWD_f, "%s ", credentials[total_creds].username);
                fscanf(passWD_f, "%s ", credentials[total_creds].password);
                fscanf(passWD_f, "%s ", credentials[total_creds].acc_type);
                
                // compare sent credentials and read credentials
                if (strcmp(userName_recv, credentials[total_creds].username) == 0)
                {
                    // user account found
                    status = 1;
                }
                
                if (!status) skip++;
                
                // update count
                total_creds++;
            }
                
            // close the file
            fclose(passWD_f);
            
            if (!status)
            {
                // re-initialize the buffer
                memset(send_buffer, '\0', BUFFER_SIZE);
                
                // Account doesn't exits, send the error response to client side
                sprintf(send_buffer, "Error: User doesn't have an account --> %s\n", userName_recv);
                write(client_socket, send_buffer, BUFFER_SIZE);
                
                // exit
                return -5;
            }
            
            // re-open the file to make modifications
            passWD_f = fopen(PASSWORD_FILE, "w");
            
            // check for errors
            if (passWD_f == NULL)
            {
                // re-initialize the buffer
                memset(send_buffer, '\0', BUFFER_SIZE);
                
                // send the error response to client side
                sprintf(send_buffer, "Error: Server failed to open password file --> %s\n", strerror(errno));
                write(client_socket, send_buffer, BUFFER_SIZE);
                
                // exit
                return -6;
            }
            
            for (int i = 0; i < total_creds; i++)
            {
                //printf("%s %s %s \n", credentials[i].username, credentials[i].password, credentials[i].acc_type);
                if (i == skip) continue;
                fprintf(passWD_f, "%s %s %s \n", credentials[i].username, credentials[i].password, credentials[i].acc_type);
            }
            
            // close the file
            fclose(passWD_f);
            
            // free up memory
            free(credentials);

            // re-initialize the buffer
            memset(send_buffer, '\0', BUFFER_SIZE);
                
            // send the success response to client side
            sprintf(send_buffer, "SUCCESS");
            write(client_socket, send_buffer, BUFFER_SIZE);

        }
        else if (option == 3)
        {
            printf("Modify User Account Selected By Admin!\n");
            char userName_recv[50];
            
            // create variable to hold new details
            PASSWORDS_ST new_details;
            
            // re-initialize the buffer
            memset(recv_buffer, '\0', BUFFER_SIZE);

            // receive username to be modified
            recv(client_socket, recv_buffer, BUFFER_SIZE, 0);
            sprintf(userName_recv, "%.49s", recv_buffer);
            
            // receive new username / password / accountType from the client
            recv(client_socket, recv_buffer, BUFFER_SIZE, 0);
            sprintf(new_details.username, "%.49s", recv_buffer);
            
            recv(client_socket, recv_buffer, BUFFER_SIZE, 0);
            sprintf(new_details.password, "%.49s", recv_buffer);
            
            recv(client_socket, recv_buffer, BUFFER_SIZE, 0);
            int account_type = atoi(recv_buffer);
            sprintf(new_details.acc_type, "%s", (account_type==0)? "USER" : "ADMIN");
            
            // declare the password file variable
            FILE *passWD_f;
            
            // open the password file to verify
            passWD_f = fopen(PASSWORD_FILE, "r");
            
            // check for errors
            if (passWD_f == NULL)
            {
                // re-initialize the buffer
                memset(send_buffer, '\0', BUFFER_SIZE);
                
                // send the error response to client side
                sprintf(send_buffer, "Error: Server failed to open password file --> %s\n", strerror(errno));
                write(client_socket, send_buffer, BUFFER_SIZE);
                
                // exit
                return -4;
            }
            
            int status = 0, skip = 0, total_creds = 0;
            int allocated = 100;
            PASSWORDS_ST *credentials = (PASSWORDS_ST *)malloc(sizeof(PASSWORDS_ST) * allocated);
            
            // loop through the passwords file and check if
            // user identity already exists or not
            while (!feof(passWD_f))
            {
                // check if more memory needs to be allocated
                if (allocated == total_creds)
                {
                    // allocate 50 more space
                    allocated += 50;
                    credentials = (PASSWORDS_ST *)realloc(credentials, sizeof(PASSWORDS_ST) * allocated);
                }
                
                // verify if credentials exist or not
                fscanf(passWD_f, "%s ", credentials[total_creds].username);
                fscanf(passWD_f, "%s ", credentials[total_creds].password);
                fscanf(passWD_f, "%s ", credentials[total_creds].acc_type);
                
                // compare sent credentials and read credentials
                if (strcmp(userName_recv, credentials[total_creds].username) == 0)
                {
                    // user account found
                    status = 1;
                }
                
                if (!status) skip++;
                
                // update count
                total_creds++;
            }
                
            // close the file
            fclose(passWD_f);
            
            if (!status)
            {
                // re-initialize the buffer
                memset(send_buffer, '\0', BUFFER_SIZE);
                
                // Account doesn't exits, send the error response to client side
                sprintf(send_buffer, "Error: User doesn't have an account --> %s\n", userName_recv);
                write(client_socket, send_buffer, BUFFER_SIZE);
                
                free(credentials);
                
                // exit
                return -5;
            }
            
            // re-open the file to make modifications
            passWD_f = fopen(PASSWORD_FILE, "w");
            
            // check for errors
            if (passWD_f == NULL)
            {
                // re-initialize the buffer
                memset(send_buffer, '\0', BUFFER_SIZE);
                
                // send the error response to client side
                sprintf(send_buffer, "Error: Server failed to open password file --> %s\n", strerror(errno));
                write(client_socket, send_buffer, BUFFER_SIZE);
                
                // exit
                return -6;
            }
            
            for (int i = 0; i < total_creds; i++)
            {
                if (i == skip)
                {
                    fprintf(passWD_f, "%s %s %s \n", new_details.username, new_details.password, new_details.acc_type);
                }
                else
                {
                    fprintf(passWD_f, "%s %s %s \n", credentials[i].username, credentials[i].password, credentials[i].acc_type);
                }
            }
            
            // close the file
            fclose(passWD_f);
            
            // free up memory
            free(credentials);

            // re-initialize the buffer
            memset(send_buffer, '\0', BUFFER_SIZE);
                
            // send the success response to client side
            sprintf(send_buffer, "SUCCESS");
            write(client_socket, send_buffer, BUFFER_SIZE);
        }
        else
        {
            // send the error response to client side
            sprintf(send_buffer, "Error: Invalid User Option --> %d\n", option);
            write(client_socket, send_buffer, BUFFER_SIZE);
            
            // print error message and exit
            return -4;
        }
    }
    
    // return from this function
    return 1;
}

int handle_user_account(int client_socket)
{
    char recv_buffer[BUFFER_SIZE], send_buffer[BUFFER_SIZE], temp_fname[100], temp_pf_full[200];
    int file_type_opt, file_size;
    
    // re-initialize the buffer
    memset(recv_buffer, '\0', BUFFER_SIZE);
    
    // receive the client's desired file type
    recv(client_socket, recv_buffer, BUFFER_SIZE, 0);
    file_type_opt = atoi(recv_buffer);
    
    if (file_type_opt == 0)
    {
        return 0;
    }
    else if ((file_type_opt == 1) || (file_type_opt == 2) || (file_type_opt == 3))
    {
        // re-initialize the buffer
        memset(recv_buffer, '\0', BUFFER_SIZE);
        
        // receive the file size
        recv(client_socket, recv_buffer, BUFFER_SIZE, 0);
        file_size = atoi(recv_buffer);
        
        // verify correct filesize was sent
        if (file_size < 0)
            return -1;
        
        // check if the directory exists otherwise make it
        struct stat st = {0};
        if (stat(TEMP_PATHNAME, &st) == -1)
            mkdir(TEMP_PATHNAME, 0700);
        
        // setup the paths variables
        sprintf(temp_fname, TEMP_FILENAME_1, rand());
        sprintf(temp_pf_full, "%s/%s", TEMP_PATHNAME, temp_fname);
        
        // open the file to write
        FILE *received_file;
        received_file = fopen(temp_pf_full, "w");
        
        // check for errors
        if (received_file == NULL)
        {
            fprintf(stderr, "Failed to open file foo --> %s\n", strerror(errno));
            return -2;
        }
        
        // start receiving the file from client
        int remain_data = file_size;
        int len = 0;
        while ((remain_data > 0) && ((len = recv(client_socket, recv_buffer, BUFFER_SIZE, 0)) > 0))
        {
                fwrite(recv_buffer, sizeof(char), len, received_file);
                remain_data -= len;
                fprintf(stdout, "Receive %d bytes and we hope :- %d bytes\n", len, remain_data);
        }
        
        // close the file
        fclose(received_file);
        
        // create the text to pdf conversion command
        char temp_command_buffer[1000];
        //temp_pf_full[strlen(temp_pf_full)-4] = '\0';
        
        if (file_type_opt == 1)
        {
            // first convert the file to postscript using enscript lib
            sprintf(temp_command_buffer, "enscript -B -p %s.ps %s", temp_pf_full, temp_pf_full);
            system(temp_command_buffer);
            
            // next convert the postscript file to pdf file
            sprintf(temp_command_buffer, "ps2pdf %s.ps %s.pdf", temp_pf_full, temp_pf_full);
            system(temp_command_buffer);
        }
        else if (file_type_opt == 2)
        {
            // convert the docx file to pdf using doc2pdf utility
            sprintf(temp_command_buffer, "doc2pdf %s %s.pdf", temp_pf_full, temp_pf_full);
            system(temp_command_buffer);
        }
        else if (file_type_opt == 3)
        {
            // first need to disable the alpha channel of the image if present
            // since img2pdf has problem with alpha channel
            sprintf(temp_command_buffer, "convert %s -background white -alpha remove -alpha off %s.png", temp_pf_full, temp_pf_full);
            system(temp_command_buffer);
            
            // nex convert the new imgae file to pdf using img2pdf utility
            sprintf(temp_command_buffer, "img2pdf %s.png -o %s.pdf", temp_pf_full, temp_pf_full);
            system(temp_command_buffer);
        }
        


        /***********************************************************
        ************************************************************
        ****                                                    ****
        ****                                                    ****
        ****      Successfully converted now send the pdf       ****
        ****      file back to client and perform cleanup       ****
        ****                                                    ****
        ****                                                    ****
        ************************************************************
        ************************************************************/
        

        

        char temp_buffer[250];
        sprintf(temp_buffer, "%s.pdf", temp_pf_full);
        
        // open the pdf file
        int send_file_fd = open(temp_buffer, O_RDONLY);
        if (send_file_fd == -1)
        {
            // re-initialize the buffer
            memset(send_buffer, '\0', BUFFER_SIZE);
            
            // send error status to server
            sprintf(send_buffer, "%d", -1);
            write(client_socket, send_buffer, BUFFER_SIZE);
            
            // print out a console message and exit
            fprintf(stderr, "Error opening file --> %s", strerror(errno));
            return -3;
        }
        
        struct stat file_stat;
        
        // get file stats
        if (fstat(send_file_fd, &file_stat) < 0)
        {
            // close the file descriptor
            close(send_file_fd);

            // re-initialize the buffer
            memset(send_buffer, '\0', BUFFER_SIZE);

            // send error status to server
            sprintf(send_buffer, "%d", -1);
            write(client_socket, send_buffer, BUFFER_SIZE);
            
            // print out a console message and exit
            fprintf(stderr, "Error fstat --> %s", strerror(errno));
            return -4;
        }

        // re-initialize the buffer
        memset(send_buffer, '\0', BUFFER_SIZE);
        
        // send the file size to server
        sprintf(send_buffer, "%ld", file_stat.st_size);
        write(client_socket, send_buffer, BUFFER_SIZE);
        
        // write the actual file to the server side
        off_t offset = 0;
        int sent_bytes = 0;
        remain_data = file_stat.st_size;
        
        // sending file data
        while (((sent_bytes = sendfile(client_socket, send_file_fd, &offset, BUFFER_SIZE)) > 0) && (remain_data > 0))
        {
                fprintf(stdout, "1. Server sent %d bytes from file's data, offset is now : %ld and remaining data = %d\n", sent_bytes, offset, remain_data);
                remain_data -= sent_bytes;
                fprintf(stdout, "2. Server sent %d bytes from file's data, offset is now : %ld and remaining data = %d\n", sent_bytes, offset, remain_data);
        }
        
        // close the file
        close(send_file_fd);
        
        // cleanup of temporary files
        sprintf(temp_command_buffer, "rm %s %s.png %s.ps %s.pdf", temp_pf_full, temp_pf_full, temp_pf_full, temp_pf_full);
        system(temp_command_buffer);
        
        /*
        if (file_type_opt == 1)
        {
            // cleanup of temporary files
            sprintf(temp_command_buffer, "rm %s %s.ps %s.pdf", temp_pf_full, temp_pf_full, temp_pf_full);
            system(temp_command_buffer);
        }
        else if (file_type_opt == 2)
        {
            // cleanup of temporary files
            sprintf(temp_command_buffer, "rm %s %s.pdf", temp_pf_full, temp_pf_full);
            system(temp_command_buffer);
        }
        */
    }

    return 1;
}

void *handler_thread(void *th_args)
{
    int client_socket = *((int *)th_args);
    char recv_buffer[BUFFER_SIZE], send_buffer[BUFFER_SIZE];

    while (1)
    {
        // print out a console message
        printf("Waiting for client to send option\n");

        // receive the client's desired operation
        recv(client_socket, recv_buffer, BUFFER_SIZE, 0);
        int option = atoi(recv_buffer);
        
        // option 0: Login - Normal-User
        // option 2: Login - Admin-User
        if ((option == 0) || (option == 2))
        {
            char userName_recv[50], userName_read[50], acctType_read[7];
            char passWord_recv[50], passWord_read[50];
            
            // receive username / password from the client
            recv(client_socket, recv_buffer, BUFFER_SIZE, 0);
            sprintf(userName_recv, "%.49s", recv_buffer);
            
            recv(client_socket, recv_buffer, BUFFER_SIZE, 0);
            sprintf(passWord_recv, "%.49s", recv_buffer);
            
            // declare the password file variable
            FILE *passWD_f;
            
            // open the password file to verify
            passWD_f = fopen(PASSWORD_FILE, "r");
            
            // check for errors
            if (passWD_f == NULL)
            {
                // re-initialize the buffer
                memset(send_buffer, '\0', BUFFER_SIZE);
                
                // send the error response to client side
                sprintf(send_buffer, "Error: Server failed to open password file --> %s\n", strerror(errno));
                write(client_socket, send_buffer, BUFFER_SIZE);
                
                // close the socket connection
                close(client_socket);
                
                // print error message and exit
                pthread_exit(NULL);
            }
            
            // create a flag to mark successful verification
            int success_flag = 0;
            
            // loop through the passwords file and verify user identity
            while (!feof(passWD_f))
            {
                // verify if credentials exist or not
                fscanf(passWD_f, "%s ", userName_read);
                fscanf(passWD_f, "%s ", passWord_read);
                fscanf(passWD_f, "%s ", acctType_read);
                
                // compare sent credentials and read credentials
                if((strcmp(userName_recv, userName_read) == 0) && (strcmp(passWord_recv, passWord_read) == 0))
                {
                    // check if the login is for administrator account
                    if ((option == 2) && (strcmp(acctType_read, "ADMIN") != 0))
                    {
                        // re-initialize the buffer
                        memset(send_buffer, '\0', BUFFER_SIZE);
                
                        // send the error response to client side
                        sprintf(send_buffer, "Error: Invalid administrator credentials\n");
                        write(client_socket, send_buffer, BUFFER_SIZE);
                        
                        // close the file
                        fclose(passWD_f);

                        // close the socket connection
                        close(client_socket);

                        // time to exit the thread
                        pthread_exit(NULL);
                    }
                    else
                    {
                        // finally we are done, close the file
                        //fclose(passWD_f);
                        
                        // update the status flag to success case
                        success_flag = 1;
                        
                        // break out of the verification loop
                        break;
                    }
                }
            }
                
            // finally we are done, close the file
            fclose(passWD_f);
            
            // check if the verification succeeded
            if (!success_flag)
            {
                // re-initialize the buffer
                memset(send_buffer, '\0', BUFFER_SIZE);
                
                // send the error response to client side
                sprintf(send_buffer, "Error: Invalid Username or Password\n");
                write(client_socket, send_buffer, BUFFER_SIZE);

                // close the socket connection
                close(client_socket);

                // time to exit the thread
                pthread_exit(NULL);
            }
            else
            {
                // re-initialize the buffer
                memset(send_buffer, '\0', BUFFER_SIZE);
                
                // send the success response to client side
                sprintf(send_buffer, "SUCCESS");
                write(client_socket, send_buffer, BUFFER_SIZE);
            }
                                
            // handle the appropriate operation
            if (option == 0)
            {
                if (handle_user_account(client_socket) != 0)
                    break;
            }
            else if (option == 2)
            {
                if(handle_admin_account(client_socket) != 0)
                    break;
            }
        }
        // register new user
        else if (option == 1)
        {
            char userName_recv[50], userName_read[50], acctType_read[7];
            char passWord_recv[50], passWord_read[50];
            
            // receive new username / password from the client
            recv(client_socket, recv_buffer, BUFFER_SIZE, 0);
            sprintf(userName_recv, "%.49s", recv_buffer);
            
            recv(client_socket, recv_buffer, BUFFER_SIZE, 0);
            sprintf(passWord_recv, "%.49s", recv_buffer);
            
            // declare the password file variable
            FILE *passWD_f;
            
            // open the password file to verify
            passWD_f = fopen(PASSWORD_FILE, "r");
            
            // check for errors
            if (passWD_f == NULL)
            {
                // re-initialize the buffer
                memset(send_buffer, '\0', BUFFER_SIZE);
                
                // send the error response to client side
                sprintf(send_buffer, "Error: Server failed to open password file --> %s\n", strerror(errno));
                write(client_socket, send_buffer, BUFFER_SIZE);
                
                // close the socket connection
                close(client_socket);
                
                // exit
                pthread_exit(NULL);
            }
            
            // loop through the passwords file and check if
            // user identity already exists or not
            while (!feof(passWD_f))
            {
                // verify if credentials exist or not
                fscanf(passWD_f, "%s ", userName_read);
                fscanf(passWD_f, "%s ", passWord_read);
                fscanf(passWD_f, "%s ", acctType_read);
                
                // compare sent credentials and read credentials
                if (strcmp(userName_recv, userName_read) == 0)
                {
                    // re-initialize the buffer
                    memset(send_buffer, '\0', BUFFER_SIZE);
                
                    // send the error response to client side
                    sprintf(send_buffer, "Error: User-account already exists\n");
                    write(client_socket, send_buffer, BUFFER_SIZE);
                            
                    // close the file
                    fclose(passWD_f);

                    // close the socket connection
                    close(client_socket);

                    // time to exit the thread
                    pthread_exit(NULL);
                }
            }
                
            // close the file
            fclose(passWD_f);
            
            // create a temp array and setup account type
            char temp_array[10];
            sprintf(temp_array, "USER");
            
            // register the new user account
            if (register_user_account(userName_recv, passWord_recv, temp_array) < 0)
            {
                // re-initialize the buffer
                memset(send_buffer, '\0', BUFFER_SIZE);
                
                // send the error response to client side
                sprintf(send_buffer, "Error: Server failed to open password file --> %s\n", strerror(errno));
                write(client_socket, send_buffer, BUFFER_SIZE);
                
                // close the socket connection
                close(client_socket);
                
                // exit
                pthread_exit(NULL);
            }
            else
            {
                // re-initialize the buffer
                memset(send_buffer, '\0', BUFFER_SIZE);
                
                // send the success response to client side
                sprintf(send_buffer, "SUCCESS");
                write(client_socket, send_buffer, BUFFER_SIZE);
            }
        }
        else if (option == 10)
        {
            // time to break from the loop
            break;
        }
        else
        {
            // re-initialize the buffer
            memset(send_buffer, '\0', BUFFER_SIZE);
                
            // send the error response to client side
            sprintf(send_buffer, "Error: Invalid User Option --> %d\n", option);
            write(client_socket, send_buffer, BUFFER_SIZE);
            
            // close the socket connection
            close(client_socket);
            
            // print error message and exit
            pthread_exit(NULL);
        }
    }

    // close the socket connection
    close(client_socket);
    
    // exit this thread
    pthread_exit(NULL);
}

int open_listen_fd(int port)
{
    // Create a socket descriptor 
    int listen_fd;
    if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        fprintf(stderr, "socket() failed\n");
        return -1;
    }
    
    // Eliminates "Address already in use" error from bind
    int optval = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, (const void *) &optval, sizeof(int)) < 0)
    {
        fprintf(stderr, "setsockopt() failed\n");
        return -1;
    }
    
    // Listen_fd will be an endpoint for all requests to port on any IP address for this host
    struct sockaddr_in server_addr;
    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET; 
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); 
    server_addr.sin_port = htons((unsigned short) port); 
    if (bind(listen_fd, (sockaddr_t *) &server_addr, sizeof(server_addr)) < 0)
    {
        fprintf(stderr, "bind() failed\n");
        return -1;
    }
    
    // Make it a listening socket ready to accept connection requests 
    if (listen(listen_fd, 1024) < 0)
    {
        fprintf(stderr, "listen() failed\n");
        return -1;
    }
    
    return listen_fd;
}

int main (int argc, char **argv)
{
    int c;

    int port = 8990;
    
    // read / process arguments provided by the user
    while ((c = getopt(argc, argv, "p:")) != -1)
    {
        switch (c) 
        {
            case 'p':
                port = atoi(optarg);
                break;
            default:
                fprintf(stderr, "usage: ./server [-p <portnum>] \n");
                exit(1);
        }
    }
    
    // now get the code to listen for connections
    int listen_fd = open_listen_fd(port);
    
    // check for any errors
    if (listen_fd == -1)
    {
        fprintf(stderr, "Error creating socket --> %s", strerror(errno));
        exit(1);
    }
    
    // print out a debug message
    printf("Info: Waiting for clients to connect!\n");
    
    // loop forever to accept client connections
    while (1)
    {
        // define all the variables needed
		struct sockaddr_in client_addr;
		int client_len = sizeof(client_addr);
		int *conn_fd = (int *)malloc(sizeof(int));
        
        // wait for a connection
        *conn_fd = accept(listen_fd, (sockaddr_t *)&client_addr, (socklen_t *)&client_len);
        
        // print out a debug message
        printf("Info: Connection received!\n");
        
        // check for any errors
        if (*conn_fd == -1)
        {
            fprintf(stderr, "Error on accept --> %s", strerror(errno));
            exit(1);
        }
        
        // create a new thread to handle this request
        pthread_t id;
        pthread_create(&id, NULL, handler_thread, (void *)conn_fd);
    }
    
    // successful exit (will never happen)
    return 0;
}