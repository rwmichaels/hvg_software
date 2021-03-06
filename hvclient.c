/* TJNAF HALLA  RCS
 * 
 * File:  hvclient.c
 * What:  All communications with the LeCroy 1458 High Voltage mainframe 
 *        or with server of control program
 *
 * This communications program will open  TCP/IP
 * connection to the HV mainframe(or server) over which commands and data
 * will be exchanged.  
 *
 *
 * R.P.: 9 Apr 2001
 * R.P.: 10 March 2016 reduce time out from 180 sec to 20 sec in  mreceive()
 * P.K.: 17 Oct 2016   add -R command line argument (Hall A Lognumber 3429691)  if the client gets duplicated lines from the server, 
 *                     and exiting with an error if the server does not conclude the communication before 1000 lines are sent
 * R.P.: 19 Oct 2016   add check (if len==0) in get_response() ; add check (if replyLen!=0) in main().  
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>

#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

/* only for HP-UX compiler, for SunOS need comment this string 
#include "./time.h"
*/
#ifdef HPVERS
 #include "./time.h"
#endif

#define MAXRBUF   256
#define MAXMLEN   80
#define HVPORT_D  1090 
#define TRUE 1
#define FALSE 0
#define RTIMEOUT  20   /* timeout in sec for waiting data in mreceive() */ 
#define MAXRESPONSE 1000

int sid,mopen;
pid_t dpid;
void daemon_start();
void usage();
void closeConnection();
int openConnection(const char *addr, int port);
int msend(char *data, int len);
int mreceive(char **buf, int *len);


char progname[MAXMLEN];
  
int main (int argc,char *argv[],char *envp[])
{ 
  int    length;
  char   data[MAXRBUF];
  int    stat;
  int    i,k;
  char   cmess[MAXRBUF];
  int    hvport;
  char   hname[MAXMLEN];
  char   cb;
  char   *rdata=NULL;
  int    rstat;
  int    flast = 0;
  int     dbg=0;
  char   *serr;
  char ch;
  int replayLen;
  char nullstr=0;
  int rc;

  char old_rdata[MAXRBUF];
  int    responseLines=0;
  int    maxResponseLines=0;

#ifdef DEBUG
 dbg=1;
#endif

  /*   for (i=0;i<argc;i++)
    {
      printf("arg %i:%s\n",i,argv[i]);
    }
   */ 


  /* save program name */
strcpy(progname,*argv);

/* default port */
hvport=HVPORT_D;

maxResponseLines=MAXRESPONSE;

if(argc==1 ) usage();

 while (--argc > 0) {
   ++argv;
   if ((*argv)[0]==(int)NULL)  usage();
   if ((*argv)[0]!= '-')  usage();
   cb = (*argv)[1];
   switch (cb) {
   case 'd':                  /* debug print on */
     dbg=1;
     break;
   case 'h':                  /* host name for connection */
     --argc;
     ++argv;
     strcpy(hname,*argv);
     if(strlen(hname)==0) usage();
     break;
   case 'p':                  /* port for connection */
     --argc;
     ++argv;
     hvport=atoi(*argv);
     break;
   case 'm':                  /* message for send to HV host */
     --argc;
     ++argv;
     strcpy(cmess,*argv);
     if(strlen(cmess)==0) usage();
     break;
   case 'R':                  /* Set maximum number of responses */
     --argc;
     ++argv;
     maxResponseLines=atoi(*argv);
     break;
   default:
     printf("ERROR: Illegal option %c \n\n",cb);
     usage();
   }
 }

 if (maxResponseLines<=0){
   maxResponseLines=MAXRESPONSE;
 }
 

/*-------------------------------------*/ 
 
      /* try to open connectioin */
 if((stat=openConnection(hname, hvport))!=0)
   { 
     perror("open connection failed");
     printf("Error open connection(PID:%d) : err-%d : errno=%d\n",dpid,stat,errno);
     exit(-1);
   }
 if(dbg)   printf("Connection succesfull!\n");

 

 strcat(cmess,&nullstr);
 
 length=strlen(cmess)+1;     

 
 memcpy(&data,cmess,length);

if(dbg) printf("message to send:<%s> len=<%d>\n",cmess,length);
  
 /* try to send message to server */
 if((stat=msend(data, length))!=0)
   {
     printf("Error send message: err:%d\n",stat);
     closeConnection();
     if(dbg)    printf("Connection Closed\n");
     exit(-1);
   }
 if(dbg) printf("Message sent out...\n... Waiting for replay\n");    
    /* get responses */
    

 do  
   {     
     flast=0;
     rdata=(char *)malloc(sizeof(cmess));
     if((stat=mreceive(&rdata,&replayLen)) < 0) 
       {
	 printf("Error receive message: err: % d\n",stat);
	 closeConnection();
	 free(rdata); 
	 if(dbg)  printf("Connection Closed\n");
	 exit(-1);
       }
       else
	 {
	   if (strncmp(rdata,old_rdata,sizeof(old_rdata))==0){
	     printf("Error, duplicated response line: newline='%s'; oldline='%s'\n",rdata, old_rdata);
	     closeConnection();
	     free(rdata); 
	     if(dbg)  printf("Connection Closed\n");
	     exit(-1);
	   } else {
	     ch = rdata[0]; 
	     if (ch == 'C') flast=1;
	     if(dbg)  printf("Len:%d  Last:%d  Data:%s\n",replayLen,flast,rdata);
	     printf("%s\n",rdata);
	     strncpy(old_rdata,rdata,sizeof(old_rdata));
	   }
	   responseLines++;
	   if (responseLines>=maxResponseLines){
	     printf("Error, terminating after receiving maximum responses (%d) from the server \n",responseLines);
	     closeConnection();
	     free(rdata); 
	     if(dbg)  printf("Connection Closed\n");
	     exit(-1);
	   }
	 }
     free(rdata);
   }
 /**  while(flast!=0); **/
 while(flast!=0 && replayLen!=0); /** 19 Oct 2016 **/
    /* close connection */
    closeConnection();
    if(dbg)  printf("Connection Closed\n");
    exit(rstat);
    
} /* end main program */



/*----------- routines -------------*/

/*---------------------------------------------------*/
/* print program usage message */
void usage(void)
{
  printf("\n%s - program to communicate with HV mainframe. \n",progname);
  printf("usage: %s -h {HV_hostname} -p {HV_Port} -m {'command_ message'}\n\n",progname);
  printf("  HV_ hostname - name(or IP address) of HV mainframe  \n");
  printf("  HV_Port      - number of HV port for TCP/IP connetction(1090 default) \n");
  printf("  'command_message' - message for sending to HV mainframe(string with quotas)\n\n");
  printf("Examples: \n");
  printf(" Send command to HV mainframe 'hallahv1':\n");
  printf("          %s -h hallahv1 -p 1090  -m 'LD L0.1 DV -1000.' \n",progname);
  printf(" Send command to control program running on 'adaqel1'\n");
  printf(" for setup voltage on HVframe 0:\n");
  printf("          %s -h adaqel1 -p 5555  -m '0 LD L0.1 DV -1000.' \n",progname);
  exit(-1);
}


/*--------------------------------------------------*/

/* Function:  openConnection()

 Given an address and a port number, this routine will attempt
 to open a bi-directional socket connection with the HV mainframe.

 Args:   const char *addr : address to connect to
                int  port : port number

 Returns:  0  == success
           1  == general error
           2  == could not create socket
           3  == could not make connection
*/
int openConnection(const char *addr, int port)
{
  int rc;
   struct sockaddr_in  sockaddr;
   struct hostent *host = gethostbyname(addr);

    if (!addr || port == 0)
      return 1;
   
   /* try to create the socket itself
    */
   sid = socket(PF_INET, SOCK_STREAM, 0);
   if (sid < 0)
      return 2;
   
   /* user may have specified the server address in either standard
    * "dot" form or an official network name like (xxx.yyy.www.eee)
    *
    * need to handle either case
   */
   if (host) {
      sockaddr.sin_family = host->h_addrtype;
      memcpy(&sockaddr.sin_addr.s_addr, host->h_addr_list[0], host->h_length);
   } else {
      sockaddr.sin_family = AF_INET;
      sockaddr.sin_addr.s_addr = inet_addr(addr);
   }
   
   sockaddr.sin_port        = htons(port);
   
   /* try to make the connection
    */
   rc = connect(sid, (struct sockaddr *)&sockaddr, sizeof(sockaddr));
   if (rc < 0)  {
      close(sid);
      return 3;
   }
   
   mopen = TRUE;
   
   return 0;
}
/*........ END openConnectiion ........ */

/* Function:  closeConnection()
 *
 *   Attempts to close the socket connection
 */
void closeConnection()
{
   if (mopen) {
      shutdown(sid,2);
      close(sid);
      mopen = FALSE;
   }
}
/*........ END closeConnectiion ........ */


/* Function:  msend()
 *
 * Attempts to send command&data to the HV mainframe
 *
 * Args:  char *data : data to send              (INPUT)
 *         int   len : number of bytes to send   (INPUT)
 *
 * Returns:  negatiive on failure, otherwise 0
 */           
int msend(char *data, int len)
{
   int rc;
   if (!mopen) return -1;
      
   /* now send the data itself
    */
   rc = send(sid, data, len, 0);
   if (rc != len)  return -2;
   
   return 0;
}
/*........ END msend ........ */

/* get_response - read response message from socket
 *
 *
 */
int get_response(int s,char *buf)
{ 
  char *buf_ptr, *buf_begin;
  char  ch;
  int len;  
  int blen=0;

  buf_begin=buf; 
  buf_ptr=buf_begin;
  do {
      /* Receive one byte at a time */
    len = recv(s,&ch,1,0);
    if (len == 1) {
      *buf_ptr = ch;
      blen++;
      buf_ptr++;
   }
    if (blen == MAXRBUF) {
      printf("Response buffer overflow.\n");
      return(-1);
    }
    /** 19 Oct 2016 **/
    if (len <= 0 ) {
      printf("Response with 0 length.\n");
      return(-1);
    }
  }   while (ch != 0);

return blen ;  
}
/*........ END get_response ........ */


/* Function:  mreceive()
 *
 * Attempts to read  bytes from 
 * the socket connection. 
 *
 * Args:  char **buf : buffer containing the received data (OUTPUT)
 *         int  &len : length of the received data         (OUTPUT)
 *
 * Notes:
 *   1) This should be made into an io_callback routine since the
 *      call will block until data is waiting on the socket.  Because
 *      of this, it's not a good idea to use this routine unless you know
 *      data is waiting on the socket or will arrive very shortly.
 *
 *   2) The 'buf' field is allocated by this routine to store the incoming
 *      data.  It is the caller's responsibility to free this memory.
 *
 * Returns:  negative on failure otherwise 0
*/
int  mreceive(char **buf, int *len)
{
   int rc;
   fd_set readSet, exceptSet;
   struct timeval timeout;
   char *serr;
   int tout=RTIMEOUT; // 180sec timeout
   int i;
   int rlen=0;
   
   timeout.tv_sec  = tout;
   timeout.tv_usec = 0;
   
   if (!mopen)
     return -1;
   
   FD_ZERO(&readSet);
   FD_ZERO(&exceptSet);
   
   FD_SET(sid, &readSet);
   FD_SET(sid, &exceptSet);
   

   /* wait until there is data waiting to be read
    */   
   
   rc = select(sid+1, (fd_set *)&readSet, 0, (fd_set *)&exceptSet, &timeout);
   if (rc < 0) 
     {
       printf("RET:rc<0\n"); return (int)NULL;
     }
   if (rc == 0) 
     {
       printf("select:exit by timeout(%d sec)\n",tout);
       return -1;
     }

   if (FD_ISSET(sid, &exceptSet)) 
     {
       printf("ERROR: exceptSet\n");
       close(sid);
       exit(-1);
     }  
      
    
   if ((rlen=get_response(sid, *buf))<0) {
     return (-1);
   }   

   *len=rlen;
 
   return(0);
}

/*........ END mreceive ........ */


/*--------------------------------*/
/* start program as daemon */
void daemon_start (void)
    {
    switch (fork ())
        {
        case 0: /* I am the child. */
	    dpid=setsid ();
            break;

        case -1: /* Failed to become daemon. */
            printf ("Can't fork()");
            exit (1);

        default: /* I am the parent. */
            exit (0);
        }
    }







