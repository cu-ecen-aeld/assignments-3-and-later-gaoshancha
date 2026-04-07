#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>

static volatile sig_atomic_t shutdown_flag = 0;
static int server_handle = -1;
static int file_handle = -1;
static int client_handle = -1;

static void signal_handler(int sig)
{
    (void)sig;
    shutdown_flag = 1;
}

int registerSignalHandler() {
  struct sigaction signalSetup;
  memset(&signalSetup, 0, sizeof(signalSetup));
  signalSetup.sa_handler = signal_handler;
  sigemptyset(&signalSetup.sa_mask);
  signalSetup.sa_flags = 0;

  if (sigaction(SIGINT,  &signalSetup, NULL) == -1 || sigaction(SIGTERM, &signalSetup, NULL) == -1) {
    syslog(LOG_ERR, "sigaction() failed: %s", strerror(errno));
    return -1;
  }

  return 0;
}

void cleanup() {
  if (server_handle != -1) close(server_handle);
  closelog();
}

int createSocket() {
  server_handle = socket(AF_INET, SOCK_STREAM, 0);
  if (server_handle == -1) {
    syslog(LOG_ERR, "Failed to create socket - socket() returned: %s", strerror(errno));
    return -1;
  }
  int opt = 1;
  if (setsockopt(server_handle, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
    syslog(LOG_ERR, "Cannot use socket in SO_REUSEADDR mode - setsockopt() returned: %s", strerror(errno));
    return -1;
  }
  return 0;
}

int bindSocket(const short tcpPort) {
  struct sockaddr_in addressConfig;
  memset(&addressConfig, 0, sizeof(addressConfig));
  addressConfig.sin_family = AF_INET;
  addressConfig.sin_addr.s_addr = INADDR_ANY;
  addressConfig.sin_port = htons(tcpPort);

  if (bind(server_handle, (struct sockaddr *)&addressConfig, sizeof(addressConfig)) == -1) {
    syslog(LOG_ERR, "Failed to bind to TCP/%d - bind() returned: %s", tcpPort, strerror(errno));
    return -1;
  }

  return 0;
}

int openListener() {
  if (listen(server_handle, 1) == -1) {
    syslog(LOG_ERR, "Cannot open listener - listen() returned: %s", strerror(errno));
    return -1;
  }
  return 0;
}

int waitForConnection(char * ipAddr) {
  struct sockaddr_in client;
  socklen_t client_len = sizeof(client);

  client_handle = accept(server_handle, (struct sockaddr *)&client, &client_len);
  if (client_handle == -1) {
    if (errno == EINTR) return -1;  
    syslog(LOG_ERR, "accept() failed: %s", strerror(errno));
    return -1;
}

  inet_ntop(AF_INET, &client.sin_addr, ipAddr, INET_ADDRSTRLEN);
  syslog(LOG_INFO, "Accepted connection from: %s", ipAddr);

  return 0;
}

void closeConnection(char * ipAddr) {
  syslog(LOG_INFO, "Closed connection from %s", ipAddr);
  close(client_handle);
}

int sendFileToClient() {
    // re-open file for reading
    int read_fd = open("/var/tmp/aesdsocketdata", O_RDONLY);
    if (read_fd == -1) {
        syslog(LOG_ERR, "Failed to open data file for reading: %s", strerror(errno));
        return -1;
    }

    char sendBuf[512];
    ssize_t n_rd;

    while ((n_rd = read(read_fd, sendBuf, sizeof(sendBuf))) > 0) {
        ssize_t n_sent = send(client_handle, sendBuf, n_rd, 0);
        if (n_sent == -1) {
            syslog(LOG_ERR, "send() failed: %s", strerror(errno));
            close(read_fd);
            return -1;
        }
    }

    if (n_rd == -1) {
        syslog(LOG_ERR, "read() failed: %s", strerror(errno));
        close(read_fd);
        return -1;
    }

    close(read_fd);
    return 0;
}

/*
void handleConnection() {
  char rxBuf[512];
  size_t rxLen = 0;
  memset(&rxBuf, 0x00, 512);

  while(1) {
    rxLen = recv(client_handle, rxBuf, sizeof(rxBuf), 0);
    if (rxLen < 0) {
      syslog(LOG_ERR, "recv() returned: %s", strerror(errno));
      break;
    }
    else if(rxLen == 0) {
      syslog(LOG_INFO, "client-side closed the connection");
      break;
    }
    else {
      char * strbuf = malloc(rxLen);
      memcpy(strbuf, rxBuf, rxLen);
      for(int i=0; i<rxLen; ++i) {
        //printf("%c", strbuf[i]);
	ssize_t n_wr = write(file_handle, &strbuf[i], 1);
        if (n_wr == -1) {
          syslog(LOG_ERR, "write() failed: %s", strerror(errno));
          break;
        }
      }
      printf("\n");
      free(strbuf);
      memset(rxBuf, 0x00, 512);
      continue;
    }
  }
}
*/
void handleConnection() {
    char rxBuf[512];
    ssize_t rxLen = 0;

    char *packetBuf = NULL;
    size_t packetLen = 0;

    while (!shutdown_flag) {
        rxLen = recv(client_handle, rxBuf, sizeof(rxBuf), 0);
        if (rxLen < 0) {
            syslog(LOG_ERR, "recv() failed: %s", strerror(errno));
            break;
        } else if (rxLen == 0) {
            syslog(LOG_INFO, "client-side closed the connection");
            break;
        }

        // append rxBuf to packetBuf
        char *tmp = realloc(packetBuf, packetLen + rxLen);
        if (tmp == NULL) {
            syslog(LOG_ERR, "realloc() failed: %s", strerror(errno));
            free(packetBuf);
            packetBuf = NULL;
            packetLen = 0;
            break;
        }
        packetBuf = tmp;
        memcpy(packetBuf + packetLen, rxBuf, rxLen);
        packetLen += rxLen;

        // check for newline — may have received multiple packets in one recv
        char *start = packetBuf;
        char *newline;
        while ((newline = memchr(start, '\n', packetLen - (start - packetBuf))) != NULL) {
            size_t lineLen = newline - start + 1;  // include the \n

            ssize_t n_wr = write(file_handle, start, lineLen);
            if (n_wr == -1) {
                syslog(LOG_ERR, "write() failed: %s", strerror(errno));
                break;
            }

            if (sendFileToClient() == -1) {
                break;
            }

            start = newline + 1;  // advance past this newline
        }

        // keep any remaining incomplete packet data
        size_t remaining = packetLen - (start - packetBuf);
        if (remaining > 0) {
            memmove(packetBuf, start, remaining);
        }
        packetLen = remaining;
    }

    free(packetBuf);
}

int daemonize() {
    pid_t pid = fork();
    if (pid < 0) {
        syslog(LOG_ERR, "fork() failed: %s", strerror(errno));
        return -1;
    }
    if (pid > 0) {
        // parent exits cleanly
        exit(EXIT_SUCCESS);
    }

    // child continues — create new session so daemon isn't
    // tied to the terminal that launched it
    if (setsid() == -1) {
        syslog(LOG_ERR, "setsid() failed: %s", strerror(errno));
        return -1;
    }

    // redirect stdin/stdout/stderr to /dev/null
    int devnull = open("/dev/null", O_RDWR);
    if (devnull == -1) {
        syslog(LOG_ERR, "Failed to open /dev/null: %s", strerror(errno));
        return -1;
    }
    dup2(devnull, STDIN_FILENO);
    dup2(devnull, STDOUT_FILENO);
    dup2(devnull, STDERR_FILENO);
    close(devnull);

    return 0;
}

int main(int argc, char *argv[])
{
    char ip_addr[INET_ADDRSTRLEN];
    int rc = 0;
    int daemon_mode = 0;
    int opt;

    while ((opt = getopt(argc, argv, "d")) != -1) {
        if (opt == 'd') daemon_mode = 1;
    }

    openlog("aesdsocket", LOG_PID, LOG_USER);
    rc = createSocket();
    if (rc) { cleanup(); return -1; }
    rc = bindSocket(9000);
    if (rc) { cleanup(); return -1; }
    rc = registerSignalHandler();
    if (rc) { cleanup(); return -1; }
    rc = openListener();
    if (rc) { cleanup(); return -1; }

    if (daemon_mode) {
        if (daemonize() == -1) {
            syslog(LOG_ERR, "Failed to daemonize: %s", strerror(errno));
            return -1;
        }
    }

    file_handle = open("/var/tmp/aesdsocketdata", O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (file_handle == -1) {
      syslog(LOG_ERR, "Failed to open data file: %s", strerror(errno));
      cleanup();
      return -1;
    }

    while (!shutdown_flag) { 
      rc = waitForConnection(ip_addr);
      if (rc) {
        if (shutdown_flag) break;  // signal interrupted accept, exit cleanly
        cleanup();
        close(file_handle);
        return -1;
      }
      handleConnection();
      closeConnection(ip_addr);
      client_handle = -1;
    }
    syslog(LOG_INFO, "Caught signal, exiting");
    if (file_handle != -1) close(file_handle);
    unlink("/var/tmp/aesdsocketdata");
    cleanup();
    return rc;
}
