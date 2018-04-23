#!/bin/usr/python

# File name: ftclient.py
# Author: Christopher Kirchner
# Class: CS362_SEII
# Date: 11/22/2016
# Description: file transfer client that get files and directory info from ftserver

import sys
import struct
from socket import *
import argparse
import os
from time import sleep


# Name: send_num
# Description: send 4-byte unsigned int through socket

def send_num(num, sock):
    return sock.send(struct.pack('!q', num))

# Name: get_num
# Description: receive 4-byte unsigned int from socket

def get_num(sock):
    #https://docs.python.org/2/library/struct.html
    return struct.unpack('!q', sock.recv(8))[0]

# Name: send_msg
# Description: sends message through socket with msg size first

def send_msg(msg, sock):
    send_num(len(msg), sock)
    sock.sendall(msg)

# Name: get_msg
# Description: receives message from socket with msg size first

def get_msg(sock):
    msg_size = get_num(sock)
    msg = ''
    # get message from socket in chunks if needed
    while msg_size > 0:
        data = sock.recv(msg_size)
        msg_size -= len(data)
        msg += data
    return msg

# Name: get_file
# Description: retrieves file from socket

def get_file(file_name, sock):
    # get file size
    file_size = get_num(sock)
    with open(file_name, 'wb') as f:
        recv = 0
        # get file in chunks
        while recv < file_size:
            chunk = sock.recv(1024)
            f.write(chunk)
            recv += len(chunk)

# Name: get_listening_sock
# Description: gets listening TCP socket bound to port

def get_listening_sock(port):
    sock = socket(AF_INET, SOCK_STREAM, 0)
    sock.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
    sock.bind(('', int(port)))
    sock.listen(1)
    return sock

# Name: get_arg_parse
# Description: gets argument parsing object for interpreting validated command line

def get_args():
    # From reading the arg parse python docs a lot
    parser = argparse.ArgumentParser(description='A simple file transfer client')
    parser.add_argument("server_host", help="server host", type=str)
    parser.add_argument("server_port", help="server port", type=str)
    parser.add_argument("-l", help="list directory", action="store_true")
    # too few args error - http://stackoverflow.com/questions/4480075/argparse-optional-positional-arguments
    parser.add_argument("-g", metavar='File', help="get file")
    parser.add_argument("data_port", help="data port", type=str)
    validate_args(parser)
    args = parser.parse_args()
    return args

# Name: validate_args
# Description: validates command line arguments using arg parser

def validate_args(parser):

    args = parser.parse_args()
    # Arg parse error messaging - http://stackoverflow.com/questions/6722936/python-argparse-make-at-least-one-argument-required
    if not args.g and not args.l:
        parser.error("no command provided")

    if int(args.server_port) < 1023 or int(args.data_port) < 1023:
        parser.error("reserve port disallowed")

    if int(args.server_port) > 65535 or int(args.data_port) > 65535:
        parser.error("impossible port provided")

    try:
        getaddrinfo(args.server_host, args.server_port)
    except gaierror as e:
        print >> sys.stderr, str(e)
        exit(1)

    # try:
    #     socket.connect_ex((args.server_host, args.server_port))
    # except Exception as e:
    #     print >> sys.stderr, str(e)

def main():

    # parse and validate command line arguments
    args = get_args()

    # interpret arguments
    command = ""
    filename = ""
    if args.l:
        command = "LIST"
    elif args.g:
        command = "GET"
        filename = args.g

    # setup ctrl sock
    ctrl_sock = socket(AF_INET, SOCK_STREAM, 0)
    # start listening for data connections
    data_server = get_listening_sock(args.data_port)

    # connect to ft server control
    ctrl_sock.connect((args.server_host, int(args.server_port)))
    # send data port
    send_msg(args.data_port, ctrl_sock)
    # send file command
    send_msg(command, ctrl_sock)
    if filename:
        send_msg(filename, ctrl_sock)

    # accept data connections from ft server
    data_sock, addr = data_server.accept()
    # print ft server directory contents
    response = get_msg(ctrl_sock)
    # act based on error or command echoed from server
    # probably a security whole to allow the client to rely on echoed server commands
    if response == "UNK_CMD":
        print >> sys.stderr, "ftclient: error: ftserver received unknown command"
        ctrl_sock.close()
        exit(1)
    elif response == "LIST_REPLY":
        lst = get_msg(data_sock)
        print lst
    # get ft server file
    elif response == "GET_REPLY":
        status = get_msg(ctrl_sock)
        # get file when server says OK
        if status == "OK":
            # handle file duplicate error
            if os.path.isfile(filename):
                print >> sys.stderr, "ftclient: error: filename is taken"
            else:
                #get file
                get_file(filename, data_sock)
                print "ftclient: success: transfer complete"
        elif status == "FAIL":
            print "ftclient: error: file not found on ftserver"

    # close sockets
    data_sock.close()
    ctrl_sock.close()

if __name__ == "__main__":
    main()