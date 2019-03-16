/* A small utility for transfering decodes from
   Pavel Demin's muilti-band FT8 receiver for the
   Red Pitaya 125-14 and 122-16 to RBN Aggregator
   for upload to the Reverse Beacon Network.
   Uses a pruned version of the WSJT-X UDP broadcast
   protocol because RBN Aggregator ignores many fields
   in the datagrams.
   By Björn Ekelund SM7IUN - March 2019 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>

const char ID[] = "STEMlab FT8 RX 1.0";

int32_t read_int(char **pointer, int32_t *value) {
  char *start = *pointer;
  *value = strtol(start, pointer, 10);
  return start != *pointer;
}

int32_t read_dbl(char **pointer, double *value) {
  char *start = *pointer;
  *value = strtod(start, pointer);
  return start != *pointer;
}

int32_t read_time(char **pointer, struct tm *value) {
  *pointer = strptime(*pointer, "%y%m%d %H%M%S", value);
  return *pointer != NULL;
}

void copy_char(char **pointer, const char *value) {
  int32_t size = strlen(value);
  int32_t rsize = htonl(size);
  memcpy(*pointer, &rsize, 4);
  *pointer += 4;
  memcpy(*pointer, value, size);
  *pointer += size;
}

void copy_int1(char **pointer, int8_t value) {
  memcpy(*pointer, &value, 1);
  *pointer += 1;
}

void copy_int4(char **pointer, int32_t value) {
  value = htonl(value);
  memcpy(*pointer, &value, 4);
  *pointer += 4;
}

void copy_double(char **pointer, double value) {
   memcpy(*pointer, &value, 8);
// Unclear if works. Ignored by RBN Aggregator.
  *pointer += 8;
}

int main(int argc, char *argv[]) {
  FILE *fp;                         // Decode file pointer
  int i;
  int sock;                         // UDP socket for broadcast
  struct sockaddr_in broadcastAddr; // Broadcast address
  int broadcastPermission = 1;	    // Socket opt to set permission to broadcast
  struct tm tm;                     // Time and date of decode
  double sync, dt;
  int32_t snr, freq, bfreq, hz, counter, rc, size;
  char buffer[512], line[64], ssnr[8], call[16], grid[8], message[32];
  char *src, *dst, *start;

  // Header including schema
  char header[8] = { 0xAD, 0xBC, 0xCB, 0xDA, 0x00, 0x00, 0x00, 0x02 };
  char msg1[4] = { 0x00, 0x00, 0x00, 0x01 };  // Message number for status datagram
  char msg2[4] = { 0x00, 0x00, 0x00, 0x02 }; // Message number for decode datagram

  unsigned short broadcastPort; // IP broadcast port
  char *broadcastIP; // IP broadcast address

  broadcastIP = argv[1];
  broadcastPort = atoi(argv[2]);

  if(argc != 4) {
    fprintf(stderr, "Usage: %s <Broadcast IP address> <Broadcast port> <Decode file>\n", argv[0]);
    return EXIT_FAILURE;
  }

  if((fp = fopen(argv[3], "r")) == NULL) {
    fprintf(stderr, "Cannot open input file.\n");
    return EXIT_FAILURE;
  }

  // Create socket for sending datagrams
  if((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
    fprintf(stderr, "Cannot open socket.\n");
    return EXIT_FAILURE;
  }

  // Set socket to allow broadcast
  if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (void *) &broadcastPermission,
      sizeof(broadcastPermission)) < 0) {
    fprintf(stderr, "Enabling broadcast failed.\n");
    close(sock);
    return EXIT_FAILURE;
  }

//  printf("broadcastIP: %s broadcastPort: %d\n", broadcastIP, broadcastPort);

  memset(&broadcastAddr, 0, sizeof(broadcastAddr)); // Zero out structure
  broadcastAddr.sin_family = AF_INET; // Address family
  broadcastAddr.sin_addr.s_addr = inet_addr(broadcastIP); // Broadcast IP address
  broadcastAddr.sin_port = htons(broadcastPort); // Broadcast IP port

// Loop until file with decodes is exhausted
  for(;;) {
    src = fgets(line, 64, fp);

    if(src != NULL) {
      rc = read_time(&src, &tm)         // Read date and time
        && read_dbl(&src, &sync)        // Read sync
        && read_int(&src, &snr)         // Read snr report
        && read_dbl(&src, &dt)          // Read timing error
        && read_int(&src, &freq)        // Read receive frequency
        && sscanf(src, "%13s %4s", call, grid); // Read call and grid

      if(!rc) continue; // Skip and do next line if parsing failed

      // Snap frequency to base frequency, round down if outside main bands
      switch ((int)(freq / 10000)) {
    	case  184: bfreq =  1840000; break;
	case  357: bfreq =  3573000; break;
	case  535: bfreq =  5357000; break;
	case  707: bfreq =  7074000; break;
	case 1013: bfreq = 10136000; break;
	case 1407: bfreq = 14074000; break;
	case 1810: bfreq = 18100000; break;
	case 2107: bfreq = 21074000; break;
	case 2491: bfreq = 24915000; break;
	case 2807: bfreq = 28074000; break;
	case 5031: bfreq = 50313000; break;
	default:   bfreq = 1000 * (int)(freq / 1000);
      } // Switch

      sprintf(ssnr, "%d", snr); // Report as string for status datagram
      hz = freq - bfreq; // Delta frequency for decode datagram
      sprintf(message, "CQ %s %s", call, grid); // Compose fake message based on decode

//      printf("Message: %s\n", message);

//      printf("call: %-13s grid: %4s sync: %5.1f freq: %8d bfreq: %8d hz: %4d dt: %4.1f snr: %3s\n",
//        call, grid, sync, freq, bfreq, hz, dt, ssnr);

      /*************************************************/
      /* Prepare status datagram                       */
      /*************************************************/

      memcpy(buffer, header, sizeof(header)); // Start with header
      dst = buffer + sizeof(header);
      memcpy(dst, msg1, sizeof(msg1)); // Message identifier
      dst += sizeof(msg1);
      copy_char(&dst, ID);       // Receiver software ID - ignored by RBNA
      copy_int4(&dst, 0);        // Base frequency as 8 byte integer
      copy_int4(&dst, bfreq);
      copy_char(&dst, "FT8");    // Rx Mode
      copy_char(&dst, call);     // DX call - ignored by RBNA
      copy_char(&dst, ssnr);     // SNR as string - ignored by RBNA
      copy_char(&dst, "FT8");    // Tx Mode - ignored by RBNA
      copy_int1(&dst, 0);        // TX enable = false - ignored by RBNA
      copy_int1(&dst, 0);        // Transmitting = false - ignorded by RBNA
      copy_int1(&dst, 0);        // Decoding = false - ignored by RBNA
      copy_int4(&dst, 0);        // rxdf - ignored by RBNA
      copy_int4(&dst, 0);        // txdf - ignored by  RBNA
      copy_char(&dst, "AB1CDE"); // DE call - ignored by RBNA
      copy_char(&dst, "AB12");   // DE grid - ignored by RBNA
      copy_char(&dst, "AB12");   // DX grid - ignored by RBNA
      copy_int1(&dst, 0);        // TX watchdog = false - ignored by RBNA
      copy_char(&dst, "");       // Submode - ignored by RBNA
      copy_int1(&dst, 0);        // Fast mode = false - ignored by RBNA
      copy_int1(&dst, 0);        // Special operation mode = 0 - ignored by RBNA

      size = dst - buffer;
//      printf("Status: size: %3d ", size);
//      printf("Message:\n"); for (i = 0; i < size; i++) printf("%02X ", buffer[i] & 0xFF); printf("\n");

      if (sendto(sock, buffer, size, 0, (struct sockaddr *)&broadcastAddr,
          sizeof(broadcastAddr)) != size) {
        fprintf(stderr, "sendto() sent a different number of bytes than expected.\n");
        return EXIT_FAILURE;
      }

      /*************************************************/
      /* Prepare status decode datagram                */
      /*************************************************/

      memcpy(buffer, header, sizeof(header)); // Header including schema information
      dst = buffer + sizeof(header);
      memcpy(dst, msg2, sizeof(msg2)); // Message identifier
      dst += sizeof(msg2);
      copy_char(&dst, ID);      // Software ID - ignored by RBNA
      copy_int1(&dst, 1);       // New decode = true
      copy_int4(&dst, 0);       // Time = zero - ignored by RBNA
      copy_int4(&dst, snr);  	// Report as 4 byte integer
      copy_double(&dst, dt); 	// Delta time - ignored by RBNA
      copy_int4(&dst, hz);      // Delta frequency in hertz - ignored by RBNA
      copy_char(&dst, "FT8");   // Receive mode - ignored by RBNA
      copy_char(&dst, message); // Fake message based on decode
      copy_int1(&dst, 0);       // Low confidence = false - ignored by RBNA
      copy_int1(&dst, 0);       // Off air = false - ignored by RBNA

      size = dst - buffer;

//      printf("Decode: size: %3d\n", size);
//      printf("Message:\n"); for (i = 0; i < size; i++) printf("%02X ", buffer[i] & 0xFF); printf("\n");

      if (sendto(sock, buffer, size, 0, (struct sockaddr *)&broadcastAddr, 
          sizeof(broadcastAddr)) != size) {
        fprintf(stderr, "sendto() sent a different number of bytes than expected.\n");
        return EXIT_FAILURE;
      }

    } // if src != NULL

    if(src == NULL) break;
  }

  fclose(fp);
  close(sock);

  return EXIT_SUCCESS;
}
