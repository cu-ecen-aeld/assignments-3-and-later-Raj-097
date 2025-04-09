#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <signal.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>
#include <sys/queue.h> // Singly linked list
#include <time.h>

#include <sys/ioctl.h>
#include "aesd_ioctl.h"


#define PORT "9000" // Port number to listen on
#define BACKLOG 10   // Maximum number of pending connections in the queue
#if USE_AESD_CHAR_DEVICE
    const char *FILE_PATH = "/dev/aesdchar";
#else
    const char *FILE_PATH = "/var/tmp/aesdsocketdata";
#endif

// Structure for thread node, used to track active client threads
typedef struct thread_node {
    pthread_t thread_id; // Thread ID
    int client_fd; // Client socket file descriptor
    SLIST_ENTRY(thread_node) entries; // Linked list entry
} thread_node_t;

// Global variables
int server_fd = -1;  // Server socket descriptor
pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER; // Mutex for file synchronization
SLIST_HEAD(thread_list, thread_node) head = SLIST_HEAD_INITIALIZER(head); // Singly linked list of threads
volatile sig_atomic_t shutdown_flag = 0; // Flag to signal shutdown request
pthread_t timestamp_thread; // Thread to append timestamps periodically


void daemonize() {
    pid_t pid = fork();
    if (pid < 0) {
        syslog(LOG_ERR, "Failed to fork process");
        exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        exit(EXIT_SUCCESS);  // Parent exits
    }

    // Create a new session and detach from the terminal
    if (setsid() < 0) {
        syslog(LOG_ERR, "Failed to create new session");
        exit(EXIT_FAILURE);
    }

    // Fork again to ensure daemon is not a session leader
    pid = fork();
    if (pid < 0) {
        syslog(LOG_ERR, "Second fork failed");
        exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        exit(EXIT_SUCCESS);  // Parent exits again
    }

    // Change working directory to root
    chdir("/");

    // Redirect standard file descriptors to /dev/null
    fclose(stdin);
    fclose(stdout);
    fclose(stderr);

    stdin = fopen("/dev/null", "r");
    stdout = fopen("/dev/null", "w");
    stderr = fopen("/dev/null", "w");
}

// Cleanup function to handle SIGINT/SIGTERM
void cleanup_and_exit(int signum) {
    syslog(LOG_INFO, "Caught signal %d, exiting", signum);
    shutdown_flag = 1; // Set shutdown flag
    close(server_fd); // Close server socket
    
    // Wait for the timestamp thread to finish execution before exiting
    pthread_join(timestamp_thread, NULL);
    
    // Join all active client threads before exiting to ensure graceful shutdown
    thread_node_t *node;
    while (!SLIST_EMPTY(&head)) {
        node = SLIST_FIRST(&head);
        pthread_join(node->thread_id, NULL);
        SLIST_REMOVE_HEAD(&head, entries);
        free(node);
    }
    #if !USE_AESD_CHAR_DEVICE
         unlink(FILE_PATH); // Remove the temporary file upon exit
    #endif
    
    pthread_mutex_destroy(&file_mutex); // Destroy the mutex to prevent memory leaks
    closelog(); // Close syslog
    exit(0);
}

void *append_timestamp(void *arg) {
    (void)arg;  // Mark argument as unused

    while (!shutdown_flag) {  
        // Instead of sleeping 10 sec at once, check shutdown_flag every 1 sec
        for (int i = 0; i < 10 && !shutdown_flag; i++) {  
            sleep(1);
        }
        if (shutdown_flag) break;  // Exit immediately if shutdown is requested

        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        char time_str[100];

        strftime(time_str, sizeof(time_str), "timestamp: %a, %d %b %Y %H:%M:%S %z\n", tm_info);

        // Lock the file to avoid race conditions
        pthread_mutex_lock(&file_mutex);
        FILE *file = fopen("/var/tmp/aesdsocketdata", "a");
        if (file) {
            fputs(time_str, file);
            fclose(file);
        } else {
            syslog(LOG_ERR, "Failed to open file for timestamp writing");
        }
        pthread_mutex_unlock(&file_mutex);
    }
    return NULL;
}


void *handle_client(void *arg) {
    thread_node_t *node = (thread_node_t *)arg;
    int client_fd = node->client_fd;
    ssize_t bytes_read;
    char recv_buffer[1024];
    char *full_msg = NULL;
    size_t total_len = 0;

    while (1) {
        // Reset message state for each new complete line
        total_len = 0;
        free(full_msg);
        full_msg = NULL;

        //_____Receive until newline is found______
        while ((bytes_read = recv(client_fd, recv_buffer, sizeof(recv_buffer), 0)) > 0) {
            char *new_buf = realloc(full_msg, total_len + bytes_read);
            if (!new_buf) {
                syslog(LOG_ERR, "Memory allocation failed");
                free(full_msg);
                close(client_fd);
                pthread_exit(NULL);
            }
            full_msg = new_buf;
            memcpy(full_msg + total_len, recv_buffer, bytes_read);
            total_len += bytes_read;

            // Check for newline
            if (memchr(recv_buffer, '\n', bytes_read)) break;
        }

        if (bytes_read == -1) {
            syslog(LOG_ERR, "Receive failed: %s", strerror(errno));
            break;
        } else if (bytes_read == 0) {
            // Client closed connection
            break;
        }
        
        // --------- HANDLE SPECIAL IOCTL COMMAND ----------
        if (strncmp(full_msg, "AESDCHAR_IOCSEEKTO:", 20) == 0) {
            unsigned int write_cmd = 0, write_cmd_offset = 0;
            if (sscanf(full_msg + 20, "%u,%u", &write_cmd, &write_cmd_offset) == 2) {
               struct aesd_seekto seekto = {
                .write_cmd = write_cmd,
                .write_cmd_offset = write_cmd_offset
               };

            pthread_mutex_lock(&file_mutex);
            int file_fd = open(FILE_PATH, O_RDWR);
            if (file_fd == -1) {
               syslog(LOG_ERR, "Failed to open device file for ioctl");
               pthread_mutex_unlock(&file_mutex);
               break;
            }

            // Perform the ioctl
            if (ioctl(file_fd, AESDCHAR_IOCSEEKTO, &seekto) == -1) {
               syslog(LOG_ERR, "ioctl failed: %s", strerror(errno));
               close(file_fd);
               pthread_mutex_unlock(&file_mutex);
               continue; // skip to next message
            }

            // Read from updated position and send back
            while ((bytes_read = read(file_fd, recv_buffer, sizeof(recv_buffer))) > 0) {
               send(client_fd, recv_buffer, bytes_read, 0);
            }
             
            lseek(file_fd, 0, SEEK_END);  // Reset position to end for future appends 
            close(file_fd);
            free(full_msg);
            full_msg = NULL;
            total_len = 0;
            pthread_mutex_unlock(&file_mutex);
            
            continue; // do not fall through to write path
        
           } else {
             syslog(LOG_ERR, "Malformed ioctl command: %s", full_msg);
             continue;
           }
        }

        // ____Write normal full message to file_____
        pthread_mutex_lock(&file_mutex);
        int file_fd = open(FILE_PATH, O_RDWR | O_CREAT | O_APPEND, 0666);
        if (file_fd == -1) {
            syslog(LOG_ERR, "Failed to open file for writing");
            pthread_mutex_unlock(&file_mutex);
            break;
        }
        ssize_t written = 0;
        while (written < (ssize_t)total_len) {
          ssize_t ret = write(file_fd, full_msg + written, total_len - written);
          if (ret == -1) {
            syslog(LOG_ERR, "write failed: %s", strerror(errno));
            break;
          }
          written += ret;
        }

        close(file_fd);
        pthread_mutex_unlock(&file_mutex);

        // Send back file contents
        pthread_mutex_lock(&file_mutex);
        file_fd = open(FILE_PATH, O_RDONLY);
        if (file_fd == -1) {
            syslog(LOG_ERR, "Failed to open file for reading");
            pthread_mutex_unlock(&file_mutex);
            break;
        }

        while ((bytes_read = read(file_fd, recv_buffer, sizeof(recv_buffer))) > 0) {
            send(client_fd, recv_buffer, bytes_read, 0);
        }

        close(file_fd);
        pthread_mutex_unlock(&file_mutex);
    }

    free(full_msg);
    close(client_fd);

    SLIST_REMOVE(&head, node, thread_node, entries);
    free(node);
    pthread_exit(NULL);
}


int main(int argc, char *argv[])
 {
 
  const char *filepath = "/var/tmp/aesdsocketdata";
    // Attempt to delete the file
    if (remove(filepath) == 0) {
        printf("File %s deleted successfully.\n", filepath);
    } else {
        // If the file doesn't exist or cannot be deleted, continue without error
        printf("File %s does not exist or cannot be deleted.\n", filepath);
    }
 int daemon_mode = 0;

    // Check for -d argument
    if (argc > 1 && strcmp(argv[1], "-d") == 0) {
        daemon_mode = 1;
    }

    struct addrinfo hints, *res;
    struct sockaddr_storage client_addr;
    socklen_t client_addr_len;
    thread_node_t *node;

    // Open syslog for logging messages
    openlog("aesdsocket", LOG_PID, LOG_USER);

    // Setup signal handlers for graceful termination
    struct sigaction sa;
    sa.sa_handler = cleanup_and_exit;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    // Configure server address
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL, PORT, &hints, &res) != 0) {
        syslog(LOG_ERR, "getaddrinfo failed");
        exit(EXIT_FAILURE);
    }

    // Create server socket
    server_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (server_fd == -1) {
        syslog(LOG_ERR, "Socket creation failed");
        freeaddrinfo(res);
        exit(EXIT_FAILURE);
    }

    // Bind the socket to the specified port
    if (bind(server_fd, res->ai_addr, res->ai_addrlen) == -1) {
        syslog(LOG_ERR, "Bind failed");
        close(server_fd);
        freeaddrinfo(res);
        exit(EXIT_FAILURE);
    }
    freeaddrinfo(res);
    
    if (daemon_mode) {
        daemonize();
    }
    pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;

// Initialize mutex before starting any thread
pthread_mutex_init(&file_mutex, NULL);
    
    #if !USE_AESD_CHAR_DEVICE
        // Create and start the timestamp thread
        pthread_create(&timestamp_thread, NULL, append_timestamp, NULL);
    #endif

    // Start listening for incoming client connections
    if (listen(server_fd, BACKLOG) == -1) {
        syslog(LOG_ERR, "Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    while (!shutdown_flag) {
        client_addr_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_fd == -1) {
            if (shutdown_flag) break;
            syslog(LOG_ERR, "Accept failed");
            continue;
        }

        node = malloc(sizeof(thread_node_t));
        if (!node) {
            syslog(LOG_ERR, "Memory allocation failed");
            close(client_fd);
            continue;
        }
        node->client_fd = client_fd;

        SLIST_INSERT_HEAD(&head, node, entries);

        pthread_create(&node->thread_id, NULL, handle_client, node);
    }

    cleanup_and_exit(0);
}
