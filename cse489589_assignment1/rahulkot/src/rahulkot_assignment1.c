/**
 * @rahulkot_assignment1
 * @author  RAHUL KOTA <rahulkot@buffalo.edu>
 * @version 1.0
 *
 * @section LICENSE
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details at
 * http://www.gnu.org/copyleft/gpl.html
 *
 * @section DESCRIPTION
 *
 * This contains the main function. Add further description here....
 */
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netdb.h>


// file descriptor for standard input
#define STDIN 0
#define PACKETSIZE 1024

struct LIST {
	char node_ipaddr[INET_ADDRSTRLEN];
	char node_port[5];
	int  node_FD;
	char node_hostname[256];
	int  node_id;
};

struct ServerLIST{
	char ipaddress[INET_ADDRSTRLEN];
	int  portno;
	char hostname[256];
};

struct ServerLIST sendServerList[5];
struct ServerLIST receivedServerList[5];
struct LIST myConnectionList[5];
int imyConnectionIndex;

// Predeclarations
void creatorInfo (void);
void getMyIP (char*);
int Upload( int iConnectionNo, char* ichFileName);
bool isPortValid (char *buf);
bool isValidIpAddress(char *ipAddress);
char* hosip (char*IP2, char* ochhostname);
void helpinfo (void);

typedef struct ClientInfo
{
	char ipAddress[16];
	int  portnumber;
} ClientInfo;

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET)
	{
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// main function
int main(int argc, char*argv[])
{
	int i =0;
	int j =0;
	int iFD =0;
	bool bserver=  false;
	bool bhasSpace = false;
    	bool bRegistered = false;
	char command_line[150];
	char command[3][32];
	char myIP[INET_ADDRSTRLEN];
	int port_num=0; 
	int startindex=0;
	int endindex =0;
	int commandindex =0;
	
	struct sockaddr_storage remoteaddr; // client address
	socklen_t addrlen;

	struct sockaddr_in my_addr;

	int myListeningSkfd; /* my socket on which I am listening*/
	int myconnectingSkfd; /* my socket on which I will be interacting with others*/
	int myMaxfd; /* my socket on which I will be interacting with others*/
	int newfd;        // newly accept()ed socket descriptor

	char chportnum[7];
	char remoteIP[INET6_ADDRSTRLEN];
	char buf[PACKETSIZE];    // buffer for client data
	int nbytes;
	int iSentBytes = 0;
	int iLengthCommand = 0;
	FILE *fpRcvdFile = NULL;
	
	//printf("\n argc: %d ",argc);
	for ( i  = 0; i< argc; i++)
	//	printf("\n option %d: %s",i, argv[i]);
	//printf("\n");
	if( isPortValid(argv[2]) != true)
	{
		printf("\n Please enter the program arguments properly ./program <s/c> <portnumber>");
		printf("\n portnumber is invalid\n");
		return -1;
	}

	// Handling Command line options	
	if( argc < 3)
	{
		printf("\n Please enter the program arguments properly ./program <s/c> <portnumber>");
		return -1;
	}
	
	//printf("\nMY Port Number is %s",argv[2]);
	
	port_num = atoi(argv[2]);
	if(strcmp(argv[1], "s") == 0)
	{
		bserver = true;
		printf("\nRunning as Server\n");
	}
	else if(strcmp(argv[1], "c")==0)
	{
		bserver = false;
		printf("\nRunning as client\n");
	}
	else
	{
		printf("\n Please enter the program arguments properly ./program <s/c> <portnumber>");
	}
	// Handling command line options ends here
	
	
	// get my ipaddress
	getMyIP(myIP);		
	//printf("IP address: %s\n",myIP );


	/*creating the socket which we use for connecting with other ip*/
	myconnectingSkfd=socket(AF_INET, SOCK_STREAM, 0);
        
	/*creating the socket for the node acting as server*/
	myListeningSkfd = socket(AF_INET, SOCK_STREAM, 0);
	if (myListeningSkfd < 0)
	{
		printf("ERROR in opening socket\n");
		return -1;
	}

	// Setting socket option
	int optval = 1;
	setsockopt(myListeningSkfd, SOL_SOCKET, SO_REUSEADDR,(const void *)&optval , sizeof(int));
	memset((char*)&my_addr, 0, sizeof(my_addr));

	//initializing my address
	my_addr.sin_family = AF_INET;
	my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	my_addr.sin_port = htons((unsigned short)port_num);
	/*binding server socket to listening port               */
	if (bind(myListeningSkfd, (struct sockaddr *) &my_addr,sizeof(my_addr)) < 0)
		perror("ERROR in binding\n");
	/*listen to 5 requests*/
	if (listen(myListeningSkfd, 5) < 0)
		perror("ERROR on listen\n");
	
	//Code for select has been taken mostly from beej's guide
	// Initialisation for select
	fd_set myread_fds;
	fd_set myinitialset;
	// clear all entries in myread_fds
	FD_ZERO(&myinitialset);
	FD_ZERO(&myread_fds);
	// adding STDINPUT to myinitialset
	FD_SET(STDIN, &myinitialset);
	// adding the socket that is being listened to myinitialset
	FD_SET(myListeningSkfd, &myinitialset);

	// getaddress info
	struct addrinfo hints, *res;
	int status;
	char ipstr[INET6_ADDRSTRLEN];
	struct sockaddr_in *ipv4 ;

	myMaxfd = myListeningSkfd;

	while(1)
	{
		printf("\nEnter Command:\n");
		myread_fds= myinitialset; // copying 

		if (select(myMaxfd+1, &myread_fds, NULL, NULL, NULL) == -1)
		{
			perror("select");
		}

		if (FD_ISSET(STDIN, &myread_fds))
		{
			// parsing command from user "REGISTER" "CREATOR" and others
			i = 0;
			bhasSpace = false;
			memset(command[0],0, 32);
			scanf("%[^\n]",command_line);

			// split commands with space ' '
			i =startindex= 0;
				commandindex = 0;
			while( command_line[i] != '\0')
			{
				if(command_line[i] == ' ')
				{
					bhasSpace = true;
					endindex = i;
					strncpy(command[commandindex],(command_line+startindex), (endindex - startindex));
					command[commandindex][endindex-startindex] = '\0';
					//printf("\n%d: %s\n",i,command[commandindex]);
					startindex = endindex + 1;
					commandindex++;

					if(commandindex == 2)
					{
						strcpy(command[commandindex], (command_line +i+1));
						break;
					}
				}    
  				i++;
			}
			if ( bhasSpace == false)
				strcpy(command[0] , command_line);
			// parsing command from usre ends here
		
			//printf("\nCommand is  %s\n",command[0]);

			// Execute the command
			if((strcmp(command[0],"EXIT")== 0) || (strcmp(command[0],"exit")==0))
			{
				for(i=0; i< 5 ; i++)
				{
					close(myConnectionList[i].node_FD);
					FD_CLR(myConnectionList[i].node_FD,&myinitialset);
				}
				break;
			}
			if(( strcmp(command[0],"HELP") == 0) || (strcmp(command[0],"help")==0))
				helpinfo();
			if(( strcmp(command[0],"CREATOR") == 0) || (strcmp(command[0],"creator")==0))
				creatorInfo();
			if( (strcmp(command[0], "MYIP") == 0) || (strcmp(command[0],"myip")==0))
				printf("IP address: %s\n",myIP );
			if(( strcmp(command[0], "MYPORT") == 0) || (strcmp(command[0],"myport")==0))
				printf("Port number:%d", port_num); 
			if(( strcmp(command[0], "REGISTER") == 0) || (strcmp(command[0],"register")==0))
			{
				if ( bserver == false)
				{
					// If already Registered
					if(bRegistered == false)
					{
					bool bvalid= isValidIpAddress(command[1]);
					if(bvalid==true)
					{
						printf("IP address is:%s",command[1]);
					}
					else
					{
						printf("\n Enter a valid IP address\n");
						//exit(1);
						
					}
					if( isPortValid(argv[2]) != true)
					{
						printf("\n portnumber is invalid\n");
					}
					
					//printf("\nportno:%s",command[2]); 
					// initializing hints
					memset(&hints, 0, sizeof hints);
					hints.ai_family = AF_UNSPEC; // AF_INET or AF_INET6 to force version
					hints.ai_socktype = SOCK_STREAM;
					if ((status = getaddrinfo(command[1], command[2], &hints, &res)) != 0) 
					{
						fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
    					}
					else
					{
						// connect to the server
						if (connect(myconnectingSkfd, res->ai_addr, res->ai_addrlen) == -1)
							printf("connection Error");
						else
						{
							FD_SET(myconnectingSkfd, &myinitialset); // add to myinitialset set
							sprintf(chportnum,"%d",port_num);
							
							// convert the IP to a string and print it:
							ipv4 = (struct sockaddr_in *)res->ai_addr;
							inet_ntop(res->ai_family, &(ipv4->sin_addr), ipstr, sizeof (ipstr));
            				//printf("IP:SUN:%s\n",ipstr);
							
							// send port number 
							send(myconnectingSkfd,chportnum,strlen(chportnum),0);
							myConnectionList[imyConnectionIndex].node_FD= 0;
							myConnectionList[imyConnectionIndex].node_id= imyConnectionIndex+1;
							strcpy(myConnectionList[imyConnectionIndex].node_ipaddr,ipstr);
							strcpy(myConnectionList[imyConnectionIndex].node_hostname,command[1]);
							strcpy(myConnectionList[imyConnectionIndex].node_port,command[2]);
							imyConnectionIndex++;
							bRegistered = true;
						}
					}
					}
				}
				else
				{
					printf("\n REGISTER Command is only applicable to Client\n");
				}

			}
			if(( strcmp(command[0], "CONNECT") == 0) || (strcmp(command[0],"connect")==0))
			{
				//The specified  IP/hostname  should be  a  valid  and  
				//listed  in  the  Server­IP­List sent to the client by the server.
				for(i = 0; i<5; i++)
				{
					if(receivedServerList[i].portno != 0)
					{
						if(strcmp(command[1],receivedServerList[i].ipaddress) == 0)
							break;
							
					}
				}
				if ( i == 5)
				{
					printf("Requested ipaddress is not in the server list\n\n");
				}
				else
				{
					// check if the connection is already established
					// Create a new socket
					int newsocket,status1;
					struct addrinfo hints1,*res1;
				
					newsocket=socket(AF_INET, SOCK_STREAM, 0);
					// initializing hints1
					memset(&hints1, 0, sizeof hints1);
					hints1.ai_family = AF_UNSPEC; // AF_INET or AF_INET6 to force version
					hints1.ai_socktype = SOCK_STREAM;
					if ((status1 = getaddrinfo(command[1], command[2], &hints1, &res1)) != 0) 
					{
						fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
	        				//return 2;
					}
					else
					{// connecting
						if (connect(newsocket, res1->ai_addr, res1->ai_addrlen) == -1)
							printf("connection Error");
						else
						{
	
							if (newsocket > myMaxfd) 
							{
								// keep track of them
								myMaxfd = newsocket;
							}
							FD_SET(newsocket, &myinitialset); // add to myinitialset set
							printf("connection Established");
							// convert the IP to a string and print it:
							ipv4 = (struct sockaddr_in *)res1->ai_addr;
							inet_ntop(res1->ai_family, &(ipv4->sin_addr), ipstr, sizeof (ipstr));
							// save the connection
							myConnectionList[imyConnectionIndex].node_FD = newsocket;
							myConnectionList[imyConnectionIndex].node_id = imyConnectionIndex+1;
							strcpy(myConnectionList[imyConnectionIndex].node_port,  command[2]);
							strcpy(myConnectionList[imyConnectionIndex].node_hostname, command[1]);
							strcpy(myConnectionList[imyConnectionIndex].node_ipaddr, ipstr);
							imyConnectionIndex++;
							sprintf(chportnum,"%d",port_num);
							send(newsocket,chportnum,strlen(chportnum),0);
						}
					}
					// establish
					// 
				}
			}
			if(( strcmp(command[0], "LIST") == 0) || (strcmp(command[0],"list")==0))
			{
				int j;
				printf("%-5s %-35s %-20s %-8s\n", "ID","Hostname" ,"IP Address", "PortNo."); 	
				for(j = 0; j < 5; j++)
				{
					if( myConnectionList[j].node_id != 0)
						printf("%-5d %-35s %-20s %-8d\n", myConnectionList[j].node_id, myConnectionList[i].node_hostname,myConnectionList[j].node_ipaddr, atoi(myConnectionList[j].node_port)); 	
				}
				printf("\n");
			

			}
			if(( strcmp(command[0], "UPLOAD") == 0) || (strcmp(command[0],"upload")==0))
			{
				// Uploading File specified	
				printf("File Name:%s\n",command[2]);
				printf("Connection: %d\n",atoi(command[1]));
				i = Upload(atoi(command[1]), command[2]);
			}
			if(( strcmp(command[0], "TERMINATE") == 0) || (strcmp(command[0],"terminate")==0))
			{
				printf("Terminating connection: %d",atoi(command[1]));
				int nConnectionNo = atoi(command[1]);
				close(myConnectionList[ (nConnectionNo -1) ].node_FD);
				FD_CLR( myConnectionList[ (nConnectionNo -1) ].node_FD, &myinitialset);
				// Update previous connections
				for( i = (nConnectionNo-1); i < (5-1); i++)
				{
					myConnectionList[i].node_FD = myConnectionList[i+1].node_FD;
					strcpy(myConnectionList[i].node_ipaddr,myConnectionList[i+1].node_ipaddr);
					strcpy(myConnectionList[i].node_port,myConnectionList[i+1].node_port);
					strcpy(myConnectionList[i].node_hostname,myConnectionList[i+1].node_hostname);
				}
				// CHECK
				myConnectionList[imyConnectionIndex].node_id = 0;
				imyConnectionIndex--;
			}
			getchar();

		}// if STDIN
		//check for connections established
		//check for new connections 

		// run through the existing connections looking for data to read

		for(iFD = 0; iFD <= myMaxfd; iFD++)
		{
			if (FD_ISSET(iFD, &myread_fds) && iFD!=STDIN) 
			{ // we got one!!
				if (iFD == myListeningSkfd)
				{
					// handle new connections
					addrlen = sizeof remoteaddr;
					newfd = accept(myListeningSkfd, (struct sockaddr *)&remoteaddr, &addrlen);
					if (newfd == -1) 
					{
						perror("accept");
					}
					else
					{
						//printf("\n ACCEPTING NEW CONNECTION\n");
						FD_SET(newfd, &myinitialset); // add to myinitialset set
						if (newfd > myMaxfd) 
						{
							// keep track of them
							myMaxfd = newfd;
						} 
						memset(buf,0 , sizeof(buf));
						// Receive listening port number of new connection
						nbytes = recv(newfd,buf,sizeof(buf),0);
						if(nbytes == -1)
							perror("RECV");

						// storing the new connection in my List myConnectionList
						//printf("my Connection Index: %d\n", imyConnectionIndex);
						sprintf(myConnectionList[imyConnectionIndex].node_ipaddr,"%s",
							inet_ntop(remoteaddr.ss_family,get_in_addr((struct sockaddr*)&remoteaddr),remoteIP,INET_ADDRSTRLEN));
						//printf("\nClient: %s",myConnectionList[imyConnectionIndex].node_ipaddr);
						sprintf(myConnectionList[imyConnectionIndex].node_port,"%s",buf);
						//printf("\nClient Listening Port: %s\n",myConnectionList[imyConnectionIndex].node_port);
						myConnectionList[imyConnectionIndex].node_FD= newfd;
						
						myConnectionList[imyConnectionIndex].node_id= imyConnectionIndex +1;
		
						// send server list
						if (bserver == true)
						{
							sendServerList[imyConnectionIndex].portno = atoi(buf);
							sprintf(sendServerList[imyConnectionIndex].ipaddress,"%s",myConnectionList[imyConnectionIndex].node_ipaddr);
							memset(buf,0 , sizeof(buf));
							sprintf(buf,"%s","PORT:");
							iLengthCommand = strlen(buf);	
							memcpy((buf+iLengthCommand),&sendServerList,(sizeof(struct ServerLIST)*5));
							for( j = 0; j< 5; j++)
							{		
								if(myConnectionList[j].node_id != 0)
								{
									iSentBytes = send(myConnectionList[j].node_FD,buf,(sizeof(struct ServerLIST)*5+iLengthCommand ),0);
									if (iSentBytes == -1)
										perror("SEND new");
									else
									{
										//printf("%d sent bytes\n",iSentBytes); 
									}
								}
							}
							//FD_SET(newfd, &myinitialset); // add to myinitialset set
/*
							while (iSentBytes != -1)
								iSentBytes = send(newfd,sendServerList,(sizeof(sendServerList)*5),0);
*/						}
						imyConnectionIndex++;
						//printf("\n CONNECTION DONE\n");
						
					}
				}
				else
				{// handle data from a client
					//printf("\n INCOMING DATA\n");
					if ((nbytes = recv(iFD, buf, sizeof( buf), 0)) <= 0)
					{// got error or connection closed by client
						if (nbytes == 0)
						{// connection closed
							printf("selectserver: socket %d hung up\n", i);
						}
						else
						{
							perror("recving ");
						}
						close(iFD); // bye!
						FD_CLR(iFD, &myinitialset); // remove from myinitialset set
						// IF server sent the updated list to all the clients
						int delportnum = 0;
						for( i = 0; i < 5; i++)
						{
							if(myConnectionList[i].node_FD == iFD)
							{
								delportnum = atoi(myConnectionList[i].node_port);
								break;
							}
						}
						if(i <5)
						{
							for( ; i < (5-1); i++)
							{
								//if(myConnectionList[i].node_FD == 0)
								//	myConnectionList[i].node_id = myConnectionList[i+1].node_id;
								myConnectionList[i].node_FD = myConnectionList[i+1].node_FD;
								strcpy(myConnectionList[i].node_ipaddr,myConnectionList[i+1].node_ipaddr);
								strcpy(myConnectionList[i].node_port,myConnectionList[i+1].node_port);
								strcpy(myConnectionList[i].node_hostname,myConnectionList[i+1].node_hostname);
							}
							// CHECK
				 			imyConnectionIndex--;
							myConnectionList[imyConnectionIndex].node_id = 0;
							if(bserver == true)
							{
								for(i = 0; i < 5;i++)
								{
									if(sendServerList[i].portno == delportnum)
									{
										sendServerList[i].portno = 0;
										break;
									}
								}
								if( i < 5)
								{
									for( ; i < (5-1); i++)
									{
										sendServerList[i].portno = sendServerList[i+1].portno;
										strcpy(sendServerList[i].ipaddress,sendServerList[i+1].ipaddress);
										strcpy(sendServerList[i].ipaddress,sendServerList[i+1].ipaddress);
									}
									/// send server list again
									memset(buf,0 , sizeof(buf));
									sprintf(buf,"%s","PORT:");
									iLengthCommand = strlen(buf);	
									memcpy((buf+iLengthCommand),&sendServerList,(sizeof(struct ServerLIST)*5));
									for( j = 0; j< 5; j++)
									{		
										if(myConnectionList[j].node_id != 0)
										{
											iSentBytes = send(myConnectionList[j].node_FD,buf,(sizeof(struct ServerLIST)*5+iLengthCommand ),0);
											if (iSentBytes == -1)
												perror("SEND new");
											else
											{
												//printf("%d sent bytes\n",iSentBytes); 
											}
										}
									}
											
								}
							
							}
						}
					}
					else
					{
						//printf("\nDATA PACKET\n");
						
						char chFileName[128]={0};
						int in = 0;
						int pktno = 0;
						int pktsize =0;
						int items =0;
						i =startindex= 0;
						char *pchTemp=NULL;
						char tempbuf[1024];
						memcpy(tempbuf,buf,1024);
						/*printf("\n");
						for( in = 0 ; in< nbytes; in++)
						{
							printf("%c",buf[in]);
							//printf("i:%d:%c ",i,chDataPacket[i]);
						}
						printf("\n");*/
						pchTemp = strtok(tempbuf, ":");
						//printf("\nInit%s %lu start %lutemp %lu\n",pchTemp,pchTemp,buf,tempbuf);
						while(pchTemp != NULL)
						{
							//printf("\n%s\n",pchTemp);
							if (strcmp(pchTemp,"PORT")==0)
							{
								// if the message start with PORT receive the ServerLIST  
								//printf("Received the list of clients from Server: %lu\n",(pchTemp+(strlen("PORT:"))));
								memcpy(receivedServerList,(pchTemp+(strlen("PORT:"))),(sizeof(struct ServerLIST)*5));
								printf("\n%-5s %-20s %-8s\n","ID","IPAddress" ,"PortNo");
								for ( in = 0; in < 5 ; in++)
								{
									if(receivedServerList[in].portno != 0)
										printf("%-5d %-20s %-8d\n",in,receivedServerList[in].ipaddress,receivedServerList[in].portno);
								}
								break;
							}
							if(strcmp(pchTemp,"RECEIVE")==0)
							{
								// if the message start with RECEIVE  receive the ServerLIST  
								pchTemp = strtok(NULL,":");
								//printf("\n%s\n",pchTemp);
								if(strcmp(pchTemp,"NAME")==0)
								{
									sprintf(chFileName,"%s",(pchTemp+(strlen("NAME:"))));
									printf("\nFILE NAME: %s\n",chFileName);
									fpRcvdFile = fopen(chFileName,"wb");
									//fclose(fpRcvdFile);
								}
								if(strcmp(pchTemp,"END")==0)
								{
									//Closing file as received all files
									fclose(fpRcvdFile);
								}
							}
							if(strcmp(pchTemp,"FILE")==0)
							{
								memcpy(&pktno,(pchTemp+(strlen("FILE:"))),sizeof(int));
								memcpy(&pktsize,(pchTemp+(strlen("FILE:"))+4),sizeof(int));
								printf("\nPacket No: %d PacketSize: %d\n",pktno,pktsize);
								
								items=fwrite((pchTemp+(strlen("FILE:"))+4+4),pktsize,1,fpRcvdFile);
								if(items==1)
								{
									//printf("SUCCESS\n");
									//fclose(fpRcvdFile);
								}
							}
							pchTemp = strtok(NULL, ":");
						}
						//printf("END\n");


					}
				} // END handle data from client
			} // END got new incoming connection
		} // END looping through file descriptors
	}

	return 0;        
}

void creatorInfo (void)
{
	printf("\nCreator  Informantion");
	printf("\nName: KOTA RAHUL");
	printf("\nUBIT Name:rahulkot");
	printf("\nUBmail:rahulkot@buffalo.edu");
	printf("\nI have read and understood the course academic integrity policy 	located at http://www.cse.buffalo.edu/faculty/dimitrio/courses/cse4589_f14/index.html#integrity");
}


void getMyIP (char * IP)
{
//Part Of this code has been taken from : http://stackoverflow.com/questions/6490759/local-ip-after-gethostbyname
	struct sockaddr_in test,test1;
int socklen;
char local[242];
int r=socket(AF_INET, SOCK_DGRAM,0);
if(r<0)
{
 printf(" MYIP socket error");
}
  test.sin_family = AF_INET;
  test.sin_port = htons(2345);
  test.sin_addr.s_addr = inet_addr("8.8.8.8");
connect(r,(struct sockaddr*)&test,sizeof test);

socklen = sizeof(test1);

getsockname(r,(struct sockaddr*)&test1,&socklen);
 inet_ntop(AF_INET,&(test1.sin_addr),local, sizeof(local));
strcpy(IP,local);  
}

bool isValidIpAddress(char *ipAddress)
{
    struct sockaddr_in sa;
    int result = inet_pton(AF_INET, ipAddress, &(sa.sin_addr));
    return result != 0;
}

// Function to upload a file to the client mentioned by the connection number
int Upload( int iConnectionNo, char* ichFileName)
{

	// File should be opened
	FILE *fpFileName;
	unsigned long uiFileSize=0;
	int i = 0;
	int UploadSkfd = 0;
	int bytes_read =0;
	int iPacketNo =0;
	int iSentBytes = 0;
	int iheaderSize = 0;
	int itemp = 0;
	int iRemaining= 0;
	int iDataSize= 0;
	char chDataPacket[PACKETSIZE];
	char *chFileName=NULL;
	char *pchTemp=NULL;
	char iCpyFileName[256];  
	char *binFileData = NULL;

	strcpy(iCpyFileName,ichFileName);
	// Open File in read binary mode
	fpFileName = fopen(ichFileName,"rb");
	if(fpFileName == NULL)
	{
		printf("\n File with name: %s\n NOTFOUND\n",ichFileName); 	
		return -1;
	}
	chFileName = ichFileName;
	pchTemp = strtok(ichFileName, "/");

	while(pchTemp != NULL)
	{
		chFileName = pchTemp;
		pchTemp = strtok(NULL, "/");
	}

	
	// use gettimeofday to get the time stamp 
	
	// To get the file size
	fseek(fpFileName, 0, SEEK_END);
	uiFileSize = ftell(fpFileName);
	rewind(fpFileName);

	binFileData = (char*)malloc(uiFileSize);
	if ( binFileData == NULL)
	{
		printf("\nUnable to allocate memory required for reading FILE\n");
		return -1;
	}
	
	bytes_read = fread(binFileData,uiFileSize, 1 , fpFileName);
	if(bytes_read != 1)
	{
		perror("Not able to read FILE");
		return -1;
	}
	
	printf("File %s contains %ld bytes!\n", ichFileName,uiFileSize);
	printf("Sending the file now...");
	//printf("File to be uploaded: %s\n", chFileName);

	//Packet format
	//  RECEIVE NAME Filename
	//  16(8+4+4) bytes 
	//  RECEIVE <4bytesPacketNo> <4bytesDataSize>  

	memset(chDataPacket, ' ', PACKETSIZE);
	sprintf(chDataPacket,"%s","RECEIVE:NAME:");		
	sprintf(chDataPacket+strlen("RECIEVE NAME:"),"%s",chFileName);
	chDataPacket[strlen("RECEIVE:NAME:")+1+strlen(chFileName)] = ':';	

	UploadSkfd = myConnectionList[iConnectionNo-1].node_FD;
	//printf("\nUpload Socket : %d\n",UploadSkfd);
	// get the socket fd and send the Data to The Socket
	//
	send(UploadSkfd,chDataPacket,PACKETSIZE,0);

	memset(chDataPacket,0, sizeof(chDataPacket));	
	sprintf(chDataPacket,"%s",":FILE:");		
	//printf("strlen of :FILE: %d",strlen(":FILE:"));		
	// Read file data into buffer.  
	// We may not have enough to fill up buffer, so we
	// store how many bytes were actually read in bytes_read.
	iPacketNo = 1;
	iheaderSize = strlen(":FILE:") + 4 +4 ;
	iDataSize = PACKETSIZE- iheaderSize;
	if(uiFileSize < iDataSize)
	{
		memcpy((chDataPacket+strlen(":FILE:")),&iPacketNo,4);
		memcpy((chDataPacket+ strlen(":FILE:")+4+4),binFileData, uiFileSize);
		
		//printf("\n FileSIZE: %d\n",uiFileSize);
		memcpy((chDataPacket+strlen(":FILE:")+4),&uiFileSize,4);
		printf("\nBefor Sending\n");
		iSentBytes =send(UploadSkfd,chDataPacket,PACKETSIZE,0);
		if(iSentBytes == -1)
		{
			perror("Sending File");
		}
		else
		{
				//printf("\n Successfully sent %d bytes\n",iSentBytes);
		}
		//send RECEIVE END to close the file
		memset(chDataPacket, ' ', PACKETSIZE);
		sprintf(chDataPacket,"%s","RECEIVE:END:");		
		send(UploadSkfd,chDataPacket,PACKETSIZE,0);
	}
	else	
	{
		// To Test
		itemp = 0;
		iRemaining = uiFileSize;
		while ( iRemaining == 0)
		{
			printf(".");
			memset(chDataPacket, ' ', PACKETSIZE);
			memcpy((chDataPacket+strlen(":FILE:")),&iPacketNo,4);
			memcpy((chDataPacket+ strlen(":FILE:")+4+4),(binFileData+itemp), iDataSize);
		
			memcpy((chDataPacket+strlen(":FILE:")+4),&iDataSize,4);
			printf("\nBefor Sending\n");
			iSentBytes =send(UploadSkfd,chDataPacket,PACKETSIZE,0);
			if(iSentBytes == -1)
			{
				perror("Sending File");
			}
			else
			{
				//printf("\n Successfully sent %d bytes\n",iSentBytes);
				iPacketNo++;
				itemp = itemp + (iSentBytes-iheaderSize);
				iRemaining = iRemaining - (iSentBytes-iheaderSize);
				if(iRemaining < iDataSize)
					iDataSize = iRemaining;
			}
		}
		memset(chDataPacket, ' ', PACKETSIZE);
		sprintf(chDataPacket,"%s","RECEIVE:END:");		
		send(UploadSkfd,chDataPacket,PACKETSIZE,0);
	}
	//printf("\n Item READ: %d %d\n",bytes_read,sizeof(chDataPacket));
	/*printf("\n");
	for( i = 0 ; i< PACKETSIZE; i++)
	{
		printf("%c",chDataPacket[i]);
		//printf("i:%d:%c ",i,chDataPacket[i]);
	}
	printf("\n");*/
	// get the socket fd and send the Data to The Socket

	return 0;
}
bool isPortValid (char *buf)
{
	int p=0;
	while((buf[p]!='\0')&& (p<5))
	{
		if ((buf[p]<'0')|| buf[p]>'9')
                         return false;
                 p++;
	}

	if( atoi(buf) <= 65536 )
    	return true;
	else
		return false;
}

bool isValidIP (char *buf)
{
	int p = 0;
	while( buf[p] != '\0' && p< 15)
	{
		if( buf[p] != '.' )
		if ((buf[p]<'0')|| buf[p]>'9')
		{
				return false;
		}
		p++;
	}
	return true;
}
char* hosip (char*IP2, char* ochhostname)
{
//part of code taken from http://cboard.cprogramming.com/linux-programming/94179-getnameinfo.html
	struct sockaddr_in pp;
	char abc[250];
	pp.sin_family = AF_INET;
	inet_pton(AF_INET, IP2, &pp.sin_addr);
	getnameinfo((struct sockaddr*)&pp, sizeof(pp),abc,sizeof(abc), NULL, 0, 0);
	strcpy(ochhostname,abc);
	return ochhostname;
}
void helpinfo (void)
{
	printf("\n creator------>displays information about creator\n myip--------->displays local IP address\n myport------->displays my portno.\n register----->this command is used only by the client for registering with the server, usage is REGISTER <server IP> <port_no>\n connect------>this command is used to connect to the destination, usage is CONNECT <destination> <port no>\n \nlist--------->list of connections active\nterminate---->This command will terminate the connection listed under the specified number when LIST is used to display all connections,Usage:teminate <id>\n\n exit--------->to exit from the process\nupload-------->The remote machine will automatically accept the file and save it under the original name in the same directory where your program is,Usage: upload <connection id> <file name>\n\ndownload------>not available\nstatistics------>not available");
}