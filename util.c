/* csci4061 F2014 Assignment 5
* section: 4
* date: 12/11/14
* names: Nicholas Weber, Amy Le
* UMN ID (weber731), (lexxx446) */

#ifndef _REENTRANT
#define _REENTRANT
#endif

#include <stdio.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <unistd.h> //newly added for fcntl.
#include <fcntl.h>  //newly added for fcntl.

#define MAXBACKLOGREQUESTS 10

static int master_fd = -1;
int megaport;
pthread_mutex_t accept_con_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t fileLock = PTHREAD_MUTEX_INITIALIZER;	// Added by NoW - 20141211 @ 0645 -- Using this in returning results/error
pthread_mutex_t requestLock = PTHREAD_MUTEX_INITIALIZER;

// this function takes a hostname and returns the IP address
int lookup_host (const char *host)
{
  struct addrinfo hints, *res;
  int errcode;
  char addrstr[100];
  void *ptr;

  memset (&hints, 0, sizeof (hints));
  hints.ai_family = PF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags |= AI_CANONNAME;

  errcode = getaddrinfo (host, NULL, &hints, &res);
  if (errcode != 0)
    {
      perror ("getaddrinfo");
      return -1;
    }

  printf ("Host: %s\n", host);
  while (res)
    {
      inet_ntop (res->ai_family, res->ai_addr->sa_data, addrstr, 100);

      switch (res->ai_family)
        {
        case AF_INET:
          ptr = &((struct sockaddr_in *) res->ai_addr)->sin_addr;
          break;
        case AF_INET6:
          ptr = &((struct sockaddr_in6 *) res->ai_addr)->sin6_addr;
          break;
        }
      inet_ntop (res->ai_family, ptr, addrstr, 100);
      printf ("IPv%d address: %s (%s)\n", res->ai_family == PF_INET6 ? 6 : 4,
              addrstr, res->ai_canonname);
      res = res->ai_next;
    }

  return 0;
}

int makeargv(const char *s, const char *delimiters, char ***argvp) {
   int error;
   int i;
   int numtokens;
   const char *snew;
   char *t;

   if ((s == NULL) || (delimiters == NULL) || (argvp == NULL)) {
      errno = EINVAL;
      return -1;
   }
   *argvp = NULL;
   snew = s + strspn(s, delimiters);
   if ((t = malloc(strlen(snew) + 1)) == NULL)
      return -1;
   strcpy(t,snew);
   numtokens = 0;
   if (strtok(t, delimiters) != NULL)
      for (numtokens = 1; strtok(NULL, delimiters) != NULL; numtokens++) ;

   if ((*argvp = malloc((numtokens + 1)*sizeof(char *))) == NULL) {
      error = errno;
      free(t);
      errno = error;
      return -1;
   }

   if (numtokens == 0)
      free(t);
   else {
      strcpy(t,snew);
      **argvp = strtok(t,delimiters);
      for (i=1; i<numtokens; i++)
         *((*argvp) +i) = strtok(NULL,delimiters);
   }
   *((*argvp) + numtokens) = NULL;
   return numtokens;
}

void freemakeargv(char **argv) {
   if (argv == NULL)
      return;
   if (*argv != NULL)
      free(*argv);
   free(argv);
}

/**********************************************
 * init
   - port is the number of the port you want the server to be
     started on
   - initializes the connection acception/handling system
   - YOU MUST CALL THIS EXACTLY ONCE (not once per thread,
     but exactly one time, in the main thread of your program)
     BEFORE USING ANY OF THE FUNCTIONS BELOW
   - if init encounters any errors, it will call exit().
************************************************/
enum boolean {FALSE, TRUE};
void init(int port) {
    printf("Entered: Init().\n");
    int enable = 1;
    int cError;
    megaport = port;
    // Create the socket. 
    master_fd = socket(AF_INET, SOCK_STREAM, 0);
    cError = master_fd;
    if(cError == -1) {
        printf("Failed: Init() -- Creating a socket\n");
        exit(-1);
        return;
    }
    printf("Success: Init() -- Creating a socket\n");

    // Create the struct addr for binding. Need this struct to cast the structure
    // pointer passed in bind to addr to avoid complier warnings.
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    //htonl and htons is for converting values between host and network byte order.
    //INADDR_ANY to bind to all local interfaces. 
    addr.sin_addr.s_addr = htons(INADDR_ANY);
    addr.sin_port = htons((short)megaport);


    // Set the socket to allow bind reuse. This is for avoiding port collision.
    cError = setsockopt(master_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&enable, sizeof(int));
    if(cError == -1) {
        printf("Failed: Init() -- Setting a socket to allow bind reuse\n");
	return;
    }
	printf("Success: Init() -- Setting a socket to allow bind reuse.\n");
    // Create the struct addr for binding. Need this struct to cast the structure
    // pointer passed in bind to addr to avoid complier warnings.
    /*struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    //htonl and htons is for converting values between host and network byte order.
    //INADDR_ANY to bind to all local interfaces. 
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((short)port);*/

    // Now need to bind the socket. Do this to assign the address to the socket.
    int error;
    error = bind(master_fd, (struct sockaddr *)&addr, sizeof(addr));
    if(error == -1) {
	printf("Failed: Init() -- Binding a socket.\n");
	// Close the socket if it failed to bind a socket.
        /*cError = close(addr);
        if(cError == -1) {
	        printf("Failed: Init() -- Closing a socket after failed bind");
        }*/
	exit(-1);
        return;
    }
	printf("Success: Init() -- Binding a socket\n");
    // Now use listen() in order to mark the newly created socket as a passive
    // socket to be used for accepting incoming connect requests.
    // Allocates queues to hold pending requests.
    cError = listen(master_fd, MAXBACKLOGREQUESTS);
    if(cError == -1) {
	    printf("Failed: Init() -- Preparing socket to accept requests\n");
    }
    printf("Finished: Init().\n");
}

/**********************************************
 * accept_connection - takes no parameters
   - returns a file descriptor for further request processing.
     DO NOT use the file descriptor on your own -- use
     get_request() instead.
   - if the return value is negative, the thread calling
     accept_connection must exit by calling pthread_exit().
***********************************************/
int accept_connection(void) {
	printf("Entered: accept_connection()\n");
    int cError;
    struct sockaddr_in addr;
    memset (&addr, 0, sizeof (addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htons(INADDR_ANY);
    addr.sin_port = htons((short) megaport);
    int addrLength = sizeof(struct sockaddr);
    int connectionFd;

    // Acquire accept_con_mutex
    pthread_mutex_lock(&accept_con_mutex);
 
    
    // Use accept() to  accept a connection request from master_fd.
    connectionFd = accept(master_fd, (struct sockaddr *)&addr, &addrLength);
    cError = connectionFd;
    if(cError == -1) {
		printf("Failed: accept_connection() -- Accepting connection\n");
		// Release accept_con_mutex
                pthread_mutex_unlock(&accept_con_mutex);
    		return -1;
    }
    printf("Success: accept_connection() -- Accepting connection\n");
    printf("Finished: accept_connection()\n");
    
    // Release accept_con_mutex
    pthread_mutex_unlock(&accept_con_mutex);
    
    return connectionFd;
}

/**********************************************
 * get_request
   - parameters:
      - fd is the file descriptor obtained by accept_connection()
        from where you wish to get a request
      - filename is the location of a character buffer in which
        this function should store the requested filename. (Buffer
        should be of size 1024 bytes.)
   - returns 0 on success, nonzero on failure. You must account
     for failures because some connections might send faulty
     requests. This is a recoverable error - you must not exit
     inside the thread that called get_request. After an error, you
     must NOT use a return_request or return_error function for that
     specific 'connection'.
************************************************/
int get_request(int fd, char *filename) {
    printf("Entered: get_request()\n");
	/*printf("Entered: get_request()\n");
	int cError, bytes;
	char * cRequest;
	cRequest = (char *)malloc(1024);
	pthread_mutex_lock(&fileLock); 
	printf("Success: get_request() -- Locking fileLock\n");




	int cFlags = 0; // The current set of flags for a given pipe
	// Extract the current set of pipe flags for child to parent
	cFlags = 0;
	cFlags = fcntl(fd, F_GETFL, 0);
	printf("Attempt: get_request -- Setting to nonblocking read.\n");
	cError = cFlags;
	if(cError == -1) {
	perror("Failed to extract cFlags for fd\n");
	return -1;
	}
	// Bitwise or the O_NONBLOCK flag onto the set of flags
	cFlags = cFlags | O_NONBLOCK;     
	cError = fcntl(fd, F_SETFL, cFlags);
	if(cError == -1) {
	perror("Failed to set fd flags\n");
	return -1;
	}
	printf("Attempt: get_request() -- Done setting to nonblocking read.\n");




	bytes = read(fd, &cRequest, sizeof(cRequest));
	printf("Attempt: get_request() -- Reading fd\n");
	cError = bytes;
	if(cError == -1) {
        printf("Failed: get_request() -- Reading fd.\n");
        return cError;
	}
	pthread_mutex_unlock(&fileLock);
	printf("Success: get_request() -- Unlocking filelock\n");
	printf("Finished: get_request()\n");
	return 0;*/

	
        pthread_mutex_lock(&requestLock);
	printf("Success: get_request() -- Locking requestLock\n");

        int cError;
	FILE *cFile;

	cFile = (fdopen(fd, "r"));
	if(cFile == NULL) {
	    printf("Failed: get_request() -- Invalid fd");
	    return -1;	
	} else {
            printf("Success: get_request() -- opened cFile with mode 'r'\n");
        }

        /*char **buffer = (char*) malloc(sizeof(char)*1024);*/
        char *cRequest = NULL;
        //makeargv(cRequest, " ", &buffer);

	size_t size = 0;
	getline(&cRequest, &size, cFile);

        /*if(fgets(cRequest, 1024, cFile) == NULL){
	  printf("Failed: get_request() -- fgets()");
	  return -1;
	}  else {
            printf("Success: get_request() -- fgets()\n");
        }*/


        printf("Success: get_request() -- getline()\n");
	cError = fflush(cFile);
	if(cError != 0) {
		printf("Failed: get_request() -- Flushing");
	} else {
	    printf("Success: get_request() -- Flushing");
        }

	//pthread_mutex_lock(&fileLock);
	//printf("Success: get_request() -- Locking requestLock\n");
	strtok(cRequest, " ");
	filename = strcpy(filename, strtok(NULL, " "));
	
	
	pthread_mutex_unlock(&requestLock);
	printf("Success: get_request() -- Unlocking requestLock\n");
	free(cRequest);

	printf("Finished: get_request()\n");
	return 0;
}

/**********************************************
 * return_result
   - returns the contents of a file to the requesting client
   - parameters:
      - fd is the file descriptor obtained by accept_connection()
        to where you wish to return the result of a request
      - content_type is a pointer to a string that indicates the
        type of content being returned. possible types include
        "text/html", "text/plain", "image/gif", "image/jpeg" cor-
        responding to .html, .txt, .gif, .jpg files.
      - buf is a pointer to a memory location where the requested
        file has been read into memory (the heap). return_result
        will use this memory location to return the result to the
        user. (remember to use -D_REENTRANT for CFLAGS.) you may
        safely deallocate the memory after the call to
        return_result (if it will not be cached).
      - numbytes is the number of bytes the file takes up in buf
   - returns 0 on success, nonzero on failure.
************************************************/
int return_result(int fd, char *content_type, char *buf, int numbytes) {
    printf("Entered: return_result()\n");
    if(content_type == NULL || buf == NULL || numbytes < 0) {
        printf("Error: return_result() -- invalid parameters\n");
        return -1;
    }

    FILE *resultFile;
    int cError = 0;

    // Acquire fileLock
    pthread_mutex_lock(&fileLock);

    // Open fd with mode "a" for appending to the end
    resultFile = fdopen(fd, "a");
    if(resultFile == NULL) {
        printf("Failed: return_result() -- open resultFile\n");
        // Release the mutex lock
        pthread_mutex_unlock(&fileLock);
        return -1;
    }

    /*  When returning a file to the web browser, you must follow the HTTP protocol.
        Specifically, if everything went OK, you should write back to the socket descriptor:
        HTTP/1.1 200 OK
        Content-Type: content-type-here
        Content-Length: num-bytes-here
        Connection: Close
        (blank line)
        File-contents-here  */

    // //// VERSION 1 //// //
    // Specific HTTP protocal message parts
    /*char *msgP1 = "HTTP/1.1 200 OK\n";
    char *msgP2 = "Content-Type: ";
    char *msgP3 = "\nContent-Length: ";
    char msgP4[numbytes];
    itoa(numbytes, msgP4, numbytes);
    char *msgP5 = "\nConnection: Close\n\n";

    // Determine the size of number of characters to write
    int msgSize =   sizeof(msgP1) + sizeof(msgP2) +
                    sizeof(content_type) + sizeof(msgP3) +
                    //numbytes + sizeof(msgP4) + sizeof(msgP5);
                    sizeof(msgP4) + sizeof(msgP5);

    // Allocate the result, which will be returned
    char *resultMsg = (char *) calloc(1, msgSize);

    // Concatenate all of the substrings together
    strncat(resultMsg, msgP1, sizeof(msgP1));
    strncat(resultMsg, msgP2, sizeof(msgP2));
    strncat(resultMsg, content_type, sizeof(content_type));
    strncat(resultMsg, msgP3, sizeof(msgP3));
    strncat(resultMsg, msgP4, sizeof(msgP4));
    strncat(resultMsg, msgP5, sizeof(msgP5));
    //strncat(resultMsg, buf, numbytes;
    printf("resultMsg:\n%s", resultMsg);

    cError = fwrite(resultMsg, 1, msgSize, resultFile);
    if(cError != msgSize) {
        printf("Failed: return_result() -- write resultMsg to resultFile\n");
        return -1;
    }*/

    // //// VERSION 2 //// //
    cError = fprintf(resultFile, "HTTP/1.1 200 OK\n");
    if(cError < 0) {
        printf("Failed: return_result() -- write HTTP protocol msg\n");
        // Release the mutex lock
        pthread_mutex_unlock(&fileLock);
        return -1;
    }

    cError = fprintf(resultFile, "Content-Type: %s\n", content_type);
    if(cError < 0) {
        printf("Failed: return_result() -- write HTTP protocol msg\n");
        // Release the mutex lock
        pthread_mutex_unlock(&fileLock);
        return -1;
    }

    cError = fprintf(resultFile, "Content-Length: %d\n", numbytes);
    if(cError < 0) {
        printf("Failed: return_result() -- write HTTP protocol msg\n");
        // Release the mutex lock
        pthread_mutex_unlock(&fileLock);
        return -1;
    }

    cError = fprintf(resultFile, "Connection: Close\n\n");
    if(cError < 0) {
        printf("Failed: return_result() -- write HTTP protocol msg\n");
        // Release the mutex lock
        pthread_mutex_unlock(&fileLock);
        return -1;
    }

    cError = fwrite(buf, 1, numbytes, resultFile);
    if(cError != numbytes) {
        printf("Failed: return_result() -- write buf to resultFile\n");
        // Release the mutex lock
        pthread_mutex_unlock(&fileLock);
        return -1;
    }

    cError = fclose(resultFile);
    if(cError != 0) {
        printf("Failed: return_result() -- close resultFile\n");
        // Release the mutex lock
        pthread_mutex_unlock(&fileLock);
        return -1;
    }

    // Release the mutex lock
    pthread_mutex_unlock(&fileLock);
    printf("Finished: return_result()\n");
    return 0;
}


/**********************************************
 * return_error
   - returns an error message in response to a bad request
   - parameters:
      - fd is the file descriptor obtained by accept_connection()
        to where you wish to return the error
      - buf is a pointer to the location of the error text
   - returns 0 on success, nonzero on failure.
************************************************/
int return_error(int fd, char *buf) {
    printf("Entered: return_error()\n");

    if(buf == NULL) {
        printf("Error: return_error() -- invalid parameters\n");
        return -1;
    }

    FILE *resultFile;
    int cError = 0;

    // Acquire fileLock
    pthread_mutex_lock(&fileLock);

    // Open fd with mode "a" for appending to the end
    resultFile = fdopen(fd, "a");
    if(resultFile == NULL) {
        printf("Failed: return_error() -- open resultFile\n");
        // Release the mutex lock
        pthread_mutex_unlock(&fileLock);
        return -1;
    }

    /*  Similarly, if something went wrong, you should write back to the socket descriptor:
        HTTP/1.1 404 Not Found
        Content-Type: text/html
        Content-Length: num-bytes-here
        Connection: Close
        (blank line)
        Error-message-here  */

    int numbytes = strlen(buf);
    cError = numbytes;
    if(cError < 0) {
        printf("Failed: return_error() -- buffer length\n");
        numbytes = 0;
    }

    /// Write error message to file
    cError = fprintf(resultFile, "HTTP/1.1 404 Not Found\n");
    if(cError < 0) {
        printf("Failed: return_result() -- write HTTP protocol msg\n");
        // Release the mutex lock
        pthread_mutex_unlock(&fileLock);
        return -1;
    }

    cError = fprintf(resultFile, "Content-Type: text/html\n");
    if(cError < 0) {
        printf("Failed: return_result() -- write HTTP protocol msg\n");
        // Release the mutex lock
        pthread_mutex_unlock(&fileLock);
        return -1;
    }

    cError = fprintf(resultFile, "Content-Length: %d\n", numbytes);
    if(cError < 0) {
        printf("Failed: return_result() -- write HTTP protocol msg\n");
        // Release the mutex lock
        pthread_mutex_unlock(&fileLock);
        return -1;
    }

    cError = fprintf(resultFile, "Connection: Close\n\n");
    if(cError < 0) {
        printf("Failed: return_result() -- write HTTP protocol msg\n");
        // Release the mutex lock
        pthread_mutex_unlock(&fileLock);
        return -1;
    }

    cError = fprintf(resultFile, "Error: 404 Not Found\n");
    if(cError < 0) {
        printf("Failed: return_result() -- write HTTP protocol msg\n");
        // Release the mutex lock
        pthread_mutex_unlock(&fileLock);
        return -1;
    }

    // Close resultFile
    cError = fclose(resultFile);
    if(cError != 0) {
        printf("Failed: return_error() -- close resultFile\n");
        // Release the mutex lock
        pthread_mutex_unlock(&fileLock);
        return -1;
    }

    // Release the mutex lock
    pthread_mutex_unlock(&fileLock);
    printf("Finished: return_error()\n");
    return 0;
}

