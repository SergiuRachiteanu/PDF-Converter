import os
import sys
import socket
import random
import tqdm
import argparse
import time
#from ctypes import *

BUFFER_SIZE = 1024

def handle_file_send(skt):
    return_status = 0
    
    try:
        while True:
            # if connected send user options to the server
            print("\nEnter the file type to send.");
            print("    0. LOG-OUT");
            print("    1. TEXT FILE (.txt)");
            print("    2. WORD FILE (.docx)");
            print("    3. IMAGE FILE (.jpg, .png)\n");
            option = int(input("Enter Option (0 - 3): "));
            
            # check for validity of the value entered
            if ((option < 0) or (option > 3)):
                print(f"Invalid User Option --> {option}")
                continue
            
            # send the first selection to server side
            send_str = f"{option}"
            skt.send(bytearray(send_str.encode()))
            
            # exit if option for logout is selected
            if option == 0:
                return_status = 1
                break;
            
            # get the file path / file name
            file_path = input("\nEnter Filepath + Filename: ");
            
            # get the file size
            send_filesize = os.path.getsize(file_path)

            # send the file size first
            send_str = f"{send_filesize}"
            skt.send(bytearray(send_str.encode()))
            
            # create a tqdm progress bar
            progress = tqdm.tqdm(range(send_filesize), f"Sending {file_path}", unit="B", unit_scale=True, unit_divisor=1024)
            
            # read and send the file in BUFFER_SIZE chunks
            with open(file_path, "rb") as f:
                
                while True:
                    
                    # read next chunk from the file
                    bytes_read = f.read(BUFFER_SIZE)
                    
                    # check if we are done transmitting
                    if not bytes_read:
                        # done with transmission
                        break
                    
                    # use the socket to send the bytes read
                    skt.send(bytes_read)
                    
                    # update the tqdm progress bar
                    progress.update(len(bytes_read))
            
            # now that the file has been sent to server let's
            # wait and fetch the pdf version of the file
            
            # fetch the file size first
            recieved_str = skt.recv(BUFFER_SIZE).decode()
            recv_filesize = int(recieved_str.split('\x00', 1)[0])
            
            # create output file path
            #if option == 1:
            output_filename = os.path.basename(file_path).rsplit(".", 1)[0] + ".pdf" # .replace(".txt", ".pdf")
            #elif option == 2:
            #    output_filename = os.path.basename(file_path).replace(".docx", ".pdf")
            #elif option == 3:
            #    output_filename = os.path.basename(file_path).replace(".docx", ".pdf")
            
            # create a tqdm progress bar
            new_progress = tqdm.tqdm(range(recv_filesize), f"Receiving {output_filename}", unit="B", unit_scale=True, unit_divisor=1024)
            
            # read and send the file in BUFFER_SIZE chunks
            with open(output_filename, "wb") as f:
                
                while True:
                    
                    # read next chunk from the file
                    bytes_read = skt.recv(BUFFER_SIZE)
                    
                    # check if we are done transmitting
                    if not bytes_read:
                        # done with reception
                        break
                    
                    # put the received bytes to file
                    f.write(bytes_read)
                    
                    # update the tqdm progress bar
                    new_progress.update(len(bytes_read))
            
            # we are done let's end the client
            break
    
    except Exception as e:
        print(str(e))
        return return_status

def main():
    
    # parse arguments
    parser = argparse.ArgumentParser(description='Python Socket Client.')
    parser.add_argument("--port", "-P", dest="port", type=int, default = 8990, help="Server Port # (default: 8990)")
    parser.add_argument("--host", "-H", dest="host", type=str, default = "127.0.0.1", help="Server Address (default: 127.0.0.1)")
    args = parser.parse_args()
    
    
    # create a socket for connection
    server_addr = (args.host, args.port)
    skt = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    
    # try to connect to this socket
    try:
        skt.connect(server_addr)
        print("Connected to {:s}".format(repr(server_addr)))
        
        while True:
            # if connected send user options to the server
            print("\nEnter the desired option #");
            print("    0. LOGIN");
            print("    1. REGISTER");
            print("    2. EXIT\n");
            option = int(input("Enter Option (0 - 2): "));
            
            # check for validity of the value entered
            if ((option < 0) or (option > 2)):
                print(f"Invalid User Option --> {option}")
                continue
            
            # send the first selection to server side
            send_str = f"{option}"
            skt.send(bytearray(send_str.encode()))
            
            # based on the selected option perform
            # desired task i.e. LOGIN or REGISTER or EXIT
            if option == 0:
                
                # get and send username
                username = input("\nENTER YOUR USERNAME: ")
                skt.send(bytearray(username.encode()))
                
                # get and send password
                passd = input("ENTER YOUR PASSWORD: ")
                skt.send(bytearray(passd.encode()))
                
                # receive response and check for success
                recv_status = (skt.recv(BUFFER_SIZE).decode()).split('\x00', 1)[0]
                if  recv_status != "SUCCESS":
                    print(f"SERVER_RESPONSE - {recv_status}")
                    break
                
                # handle the file sending request
                if handle_file_send(skt) == 1:
                    continue
                else:
                    break;
                
            elif option == 1:
                
                # get and send desired username
                username = input("\nENTER YOUR DESIRED USERNAME: ")
                skt.send(bytearray(username.encode()))
                
                # get and send password
                passd = input("ENTER YOUR DESIRED PASSWORD: ")
                skt.send(bytearray(passd.encode()))
                
                # receive response and check for success
                recv_status = skt.recv(BUFFER_SIZE).decode()
                if  recv_status != "SUCCESS":
                    print(f"SERVER_RESPONSE - {recv_status}")
                    # time to exit, break out of loop
                    break;
                else:
                    print("\nNew user added successfully.\n")

            elif option == 2:
                break

    except AttributeError as ae:
        print("Error creating the socket: {}".format(ae))
    except socket.error as se:
        print("Exception on socket: {}".format(se))
    finally:
        print("Closing socket")
        skt.close()


if __name__ == "__main__":
    main()