This package includes:

-chat.c [This is the driver program. Upon running, it will act as a server or client for a text messager. Client messages first and messages are packed with version number and message length. Messaging runs until one user terminates(^c).]
-README.txt [This]

To compile:
    make

To run:
	This will start the server and display IP and Port to connect to:
	./chat
	This will start the client:
    ./chat  -s <IP address> -p <Port>
    This and anything else will produce the usage:
    ./chat -h

Example:
	./chat -p 1701 -s 53.95.116.113

