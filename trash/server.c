#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>


#define HOST 127.0.0.1
#define START_PORT 8080
#define ROWS 8
#define COLUMNS 8
#define USPF 1070000
#define SPF 1.07
#define TILE_COUNT 64
#define GOP_COUNT 10
#define BUFFER_SIZE 64000

char header[] = {
	0x00, 0x00, 0x00, 0x01, 0x40, 0x01, 0x0c, 0x01, 0xff, 0xff, 0x01, 0x60, 0x00, 0x00, 0x03, 0x00,
	0x90, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x5a, 0x95, 0x98, 0x09, 0x00, 0x00, 0x00, 0x01,
	0x42, 0x01, 0x01, 0x01, 0x60, 0x00, 0x00, 0x03, 0x00, 0x90, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03,
	0x00, 0x5a, 0xa0, 0x07, 0x82, 0x01, 0xe1, 0x65, 0x95, 0x9a, 0x49, 0x32, 0xb8, 0x04, 0x00, 0x00
};

struct thread_args {
  int tile_num;
};

void createDirectory(char * pathname) {
  int res = 0;
  struct stat st = {
    0
  };
  if (stat(pathname, & st) == -1) {
    if (mkdir(pathname, 0755) < 0) {
      perror("error creating directory");

    }
  }
}
void * ackThread() {
  int server_sock = 0, len = 0, n = 0;
  char buffer[BUFFER_SIZE];
  struct sockaddr_in servaddr, cliaddr;
  struct timeval timeout;

  // set timeout to 1.07 seconds
  timeout.tv_sec = 1;
  timeout.tv_usec = 70000;
  len = sizeof(cliaddr);

  // create socket file descriptor
  server_sock = socket(AF_INET, SOCK_DGRAM, 0);
  // configure server socket
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(START_PORT + 100);
  servaddr.sin_addr.s_addr = inet_addr("192.168.0.2");

  // bind the socket to the specified port
  bind(server_sock, (struct sockaddr * ) & servaddr, sizeof(servaddr));
  //setsockopt(server_sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

  // listen for packet from client then send response to client to calculate bandwidth
  while (1) {
    n = recvfrom(server_sock, buffer, BUFFER_SIZE, MSG_WAITALL, (struct sockaddr * ) & cliaddr, & len);
    if (n == BUFFER_SIZE) {
      return 0;
      buffer[n] = '\0';
      sendto(server_sock, buffer, BUFFER_SIZE, 0, (struct sockaddr * ) & cliaddr, sizeof(servaddr));
      memset(buffer, 0, sizeof(buffer));
    }
  }
}

/*
 * calculate the row / column value based on the tile number
 * and store value as a string
 */
void setRowCol(char * row, char * col, int tile_num) {
  sprintf(row, "%d", (int)((tile_num) / COLUMNS) + 1);
  sprintf(col, "%d", (tile_num % COLUMNS) + 1);
}

/*
 * The memmem() function finds the start of the first occurrence of the
 * substring 'needle' of length 'nlen' in the memory area 'haystack' of
 * length 'hlen'.
 *
 * The return value is a pointer to the beginning of the sub-string, or
 * NULL if the substring is not found.
 */
void * memmem(const void * haystack, size_t hlen, const void * needle, size_t nlen, int * hIndex) {
  int needle_first, index = 0;
  const void * p = haystack;
  size_t plen = hlen;
  * hIndex = -1;
  if (!nlen)
    return NULL;
  needle_first = * (unsigned char * ) needle;
  while (plen >= nlen && (p = memchr(p, needle_first, plen - nlen + 1))) {
    if (!memcmp(p, needle, nlen)) {
      * hIndex = (int)(p - haystack);
      return (void * ) p;
    }
    p++;
    index += 1;
    plen = hlen - (p - haystack);
  }
  return NULL;
}

char * setFilename(char * filename, char * gop_num, char * tile_num, char * row, char * col) {
  memset(filename, 0, sizeof(filename));
  strcat(filename, "./received/");
  strcat(filename, gop_num);
  strcat(filename, "_");
  strcat(filename, row);
  strcat(filename, "-");
  strcat(filename, col);
}

/* set the read timeout */
void setTimeout(int server_sock, double elapsed_time) {
  struct timeval timeout;
  if (elapsed_time <= SPF - 1) {
    timeout.tv_sec = 1;
    timeout.tv_usec = (int)(((SPF - 1) - elapsed_time) * 1000000);
  } else if (elapsed_time > SPF - 1) {
    timeout.tv_sec = 0;
    timeout.tv_usec = (int)((SPF - elapsed_time) * 1000000);
  }
  //setsockopt(server_sock, SOL_SOCKET, SO_RCVTIMEO, (char * ) & timeout, sizeof(timeout));
}

/* split the new and previous gop data */
void splitBuffer(char * headerPtr, char * buffer, char * newFileBuffer, char * prevFileBuffer, int hIndex, int bytes) {
  char * ptr = NULL;
  // copy the part of the buffer that contains the new file to newFileBuffer
  memcpy(newFileBuffer, headerPtr, bytes-hIndex);
  // copy the part of the buffer that contains the current file to prevFileBuffer
  if (hIndex > 0) {
    ptr = & buffer[0];
    memcpy(prevFileBuffer, ptr, hIndex);
  }
}

void writeNewFile(FILE * fp, char * buffer, char * filename, char * gop_num, char * tile_num, char * row, char * col, int curr_gop, int bytes, int hIndex) {
  // create new file to begin writing to it
  sprintf(gop_num, "%d", curr_gop);
  setFilename(filename, gop_num, tile_num, row, col);
  strcat(filename, ".bin");
  fp = fopen(filename, "wb");
  fwrite(buffer, 1, bytes - hIndex, fp);
}

void savePrevFile(FILE * fp, char * buffer) {
  fwrite(buffer, 1, sizeof(buffer), fp);
  fclose(fp);
  fp = NULL;
}

void closeFile(FILE * fp) {
  fclose(fp);
  fp = NULL;
}

void clearBuffers(char * buffer1, char * buffer2, char * buffer3) {
  memset(buffer1, 0, sizeof(buffer1));
  memset(buffer2, 0, sizeof(buffer2));
  memset(buffer3, 0, sizeof(buffer3));
}

int getGOP(int server_sock, char * tile_num, char * row, char * col) {
  int bytes = 0, curr_gop = 0, len = 0, flag = 0, hIndex = 0;
  double packet_start = 0.0, packet_time = 0.0, elapsed_time = 0.0;
  char filename[1024], buffer[BUFFER_SIZE], newFileBuffer[BUFFER_SIZE], prevFileBuffer[BUFFER_SIZE];
  char gop_num[5];
  char * headerPtr = NULL;
  FILE * fp = NULL;
  struct sockaddr_in cliaddr;
  // read in the given file for every frame
  while (curr_gop <= GOP_COUNT) {
    if (flag == 0) {
      recvfrom(server_sock, buffer, 1, MSG_PEEK, (struct sockaddr * ) & cliaddr, & len);
      flag = 1;
    }
		//setTimeout(server_sock, elapsed_time);
    bytes = 0;
    // clear buffers
    clearBuffers(buffer, newFileBuffer, prevFileBuffer);
    packet_start = time(NULL);
    bytes = recvfrom(server_sock, buffer, BUFFER_SIZE, 0, (struct sockaddr * ) & cliaddr, & len);
    packet_time = time(NULL) - packet_start;
    if (bytes > 0) {
			//printf("got %d bytes from %s\n", bytes, filename);
      // return pointer to the beginning of substring (needle)
      headerPtr = memmem(buffer, sizeof(buffer), header, sizeof(header), & hIndex);
			if (hIndex > 0) {
				printf("%s starts at %d\n", filename, hIndex);
			}
      // receiving new file
      if (headerPtr != NULL) {
				splitBuffer(headerPtr, buffer, newFileBuffer, prevFileBuffer, hIndex, bytes);
        if (fp != NULL) {
          if (hIndex > 0) {
						printf("messy!\n");
            //savePrevFile(fp, prevFileBuffer);
						fwrite(prevFileBuffer, 1, hIndex, fp);
						//printf("(1) writing %d to %s\n", hIndex, filename);
          }
					fclose(fp);
					fp = NULL;
        }
        // not only sent header
        if ((bytes - hIndex) > sizeof(header)) {
          //writeNewFile(fp, buffer, filename, gop_num, tile_num, row, col, curr_gop, bytes, hIndex);
					sprintf(gop_num, "%d", curr_gop);
					setFilename(filename, gop_num, tile_num, row, col);
					strcat(filename, ".bin");
					fp = fopen(filename, "wb");
					fwrite(newFileBuffer, 1, bytes - hIndex, fp);
					//printf("(2) writing %d to %s\n", bytes - hIndex, filename);
        }
        // only new file
        if (hIndex == 0 && (bytes - hIndex) > sizeof(header)) {
          elapsed_time = packet_time;
        }
        // current file + new file
        else {
          elapsed_time = elapsed_time + packet_time;
          if (elapsed_time < SPF) {
            sleep(SPF - elapsed_time);
          }
          elapsed_time = 0.0;
        }
        curr_gop += 1;
      }
      // add to previous file
      else if (bytes > 0 && fp != NULL) {
        fwrite(buffer, 1, bytes, fp);
				//printf("(3) writing %d to %s\n", bytes, filename);
        elapsed_time = elapsed_time + packet_time;
      }
    }
		else if (bytes < 0) {
			elapsed_time = 0;
			if (fp != NULL) {
				fclose(fp);
				fp = NULL;
			}
		}
    // if we timeout, then restart elapsed time and start sending next frame
    if (elapsed_time >= SPF && curr_gop > 0) {
			//printf("leaving from %s early\n", filename);
      elapsed_time = 0.0;
      if (fp != NULL) {
				fclose(fp);
				fp = NULL;
			}
    }
  }
  return 0;
}

void * receiveThread(void * arguments) {
  int server_sock = 0, i = 0;
  char gop_num[5], tile_num[5], row[5], col[5];
  struct sockaddr_in servaddr;
  struct thread_args * args = arguments;
  // Creating socket file descriptor
  if ((server_sock = socket(AF_INET, SOCK_DGRAM, 0)) == 0) {
    perror("socket failed");
    exit(EXIT_FAILURE);
  }
  // configure server socket
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(START_PORT + args->tile_num);
  servaddr.sin_addr.s_addr = inet_addr("192.168.0.2");
  // bind the socket to the specified port
  if (bind(server_sock, (struct sockaddr * ) & servaddr, sizeof(servaddr)) < 0) {
    perror("bind failed");
    exit(EXIT_FAILURE);
  }
  /* listens for each successive frame
  	 in 1.07 second intervals
  */
  // set the row / column values
  setRowCol(row, col, args->tile_num);
  sprintf(tile_num, "%d", args->tile_num);
  // listen for messages at specified port
  getGOP(server_sock, tile_num, row, col);
}

int main(int argc, char
  const * argv[]) {
  int i = 0;
  pthread_t thread_array[TILE_COUNT], ack; // array for each thread
  struct thread_args args[TILE_COUNT]; // array for argument structs

  /* create udp threads for listening at each port for the corresponding tile */
  for (i = 0; i < TILE_COUNT; i++) {
    args[i].tile_num = i;
    /* create thread to send videos */
    pthread_create( & thread_array[i], NULL, & receiveThread, (void * ) & args[i]);
  }
  pthread_create( & ack, NULL, ackThread, NULL);
  /* wait for threads to end */
  for (i = 0; i < TILE_COUNT; i++) {
    pthread_join(thread_array[i], NULL);
  }
  return 0;
}