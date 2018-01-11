The purpose of this project was to implement both GBN and Selective Repeat transfer protocols, and provide 
simple application layer commands to demonstrate them.

Build and Run:
	To build the project, run "make all" in the command line.
	To run the server, run: "./server gbn windowSize lossRate corruptionRate". Next, start the server with /start
	To run the client, run: "./client gbn windowSize lossRate corruptionRate". and type /connect localhost and /help for available commands. 

The commands available are as follows:
	"/connect hostname" - command for the client side to connect to the server
	"/start" - command on server side that starts the server
	"/transfer path/to/file" - transfer the specified message to the other person
	"/chat message" - sends the specified message to the other person
	"/stats" - writes stats to output folder in the "log" directory. Creates two files, a sender and reciever stats file
	"/help" - displays the commands available for client or sever
	"/exit" - exits the program



