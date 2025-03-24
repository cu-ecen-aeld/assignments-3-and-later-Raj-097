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

#define PORT "9000" // Port number to listen on
#define BACKLOG 10   // Maximum number of pending connections in the queue
#define FILE_PATH "/var/tmp/aesdsocketdata" // Path to store received data
#define DEBUG_LOG_FILE "/var/tmp/aesdsocket_debug.log"


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

void debug_log(const char *message) {
    FILE *file = fopen(DEBUG_LOG_FILE, "a");
    if (file) {
        fprintf(file, "%s\n", message);
        fflush(file);  // Ensure message is written immediately
        fclose(file);
    }
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
    
    unlink(FILE_PATH); // Remove the temporary file upon exit
    debug_log("Deleted aesdsocketdata file");
    pthread_mutex_destroy(&file_mutex); // Destroy the mutex to prevent memory leaks
    closelog(); // Close syslog
    debug_log("aesdsocket exiting.");
    exit(0);
}

void *append_timestamp(void *arg) {
    (void)arg;  // Mark argument as unused

    while (1) {
        sleep(10);  // Wait for 10 seconds

        // Get the current time
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


// Thread function to handle client communication
void *handle_client(void *arg) {
    debug_log("Changes are reflected in the build");
    thread_node_t *node = (thread_node_t *)arg;
    int client_fd = node->client_fd;
    char buffer[1024];
    ssize_t bytes_read;
    char log_msg[128]; // for snprintf


    // Open the file for appending client data
    int file_fd = open(FILE_PATH, O_RDWR | O_CREAT | O_APPEND, 0666);
    if (file_fd == -1) {
        syslog(LOG_ERR, "Failed to open file");
        close(client_fd);
        pthread_exit(NULL);
    }

    // Lock file access
    pthread_mutex_lock(&file_mutex);
    // Read data from client
    while ((bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes_read] = '\0';  // Null-terminate received data
        char *newline_pos = strchr(buffer, '\n');
        if (newline_pos) {
            size_t length_to_write = newline_pos - buffer + 1; // Include '\n'
            write(file_fd, buffer, length_to_write);
            break;
        } else {
            write(file_fd, buffer, bytes_read);
        }
    }
    // Handle read errors
    if (bytes_read == -1) {
        syslog(LOG_ERR, "Receive failed: %s", strerror(errno));
    }
    // Close the file after writing
    close(file_fd);
    // Unlock after writing is done
    pthread_mutex_unlock(&file_mutex);
    
    debug_log("Sending file content now");
    // Lock again before reading the file
    pthread_mutex_lock(&file_mutex);
    // Reopen file for reading
    file_fd = open(FILE_PATH, O_RDONLY);
    // Now, read the whole file and send it back
    if (file_fd != -1) {
        // Move to the beginning of the file before reading
        lseek(file_fd, 0, SEEK_SET);
        debug_log("lseek compiled");
        
        ssize_t file_bytes;
        while ((file_bytes = read(file_fd, buffer, sizeof(buffer))) > 0) {
            ssize_t total_sent = 0;
    
            while (total_sent < file_bytes) {  
                ssize_t bytes_sent = send(client_fd, buffer + total_sent, file_bytes - total_sent, 0);
        
                if (bytes_sent == -1) {
            	   snprintf(log_msg, sizeof(log_msg), "Send failed: %s", strerror(errno));
            	   debug_log(log_msg);
                   break;  // Exit loop on failure
        	}
        
        	total_sent += bytes_sent;
        	snprintf(log_msg, sizeof(log_msg), "Sent %zd/%zd bytes", total_sent, file_bytes);
        	debug_log(log_msg);
    	    }
        }
        
        close(file_fd);
    } else {
    	debug_log("Failed to open file for reading");
        syslog(LOG_ERR, "Failed to open file for reading");
    }
    
    // Unclock file access
    pthread_mutex_unlock(&file_mutex);
    debug_log("File content sent");
    
    // Remove the thread from the list and free allocated memory
    SLIST_REMOVE(&head, node, thread_node, entries);
    free(node);
    close(client_fd);
    client_fd = -1;
    file_fd = -1;
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
    
    

    // Start listening for incoming client connections
    if (listen(server_fd, BACKLOG) == -1) {
        syslog(LOG_ERR, "Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    int timestamps_started = 0; // Flag to ensure timestamp thread runs only once
    while (!shutdown_flag) {
        client_addr_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_fd == -1) {
            if (shutdown_flag) break;
            syslog(LOG_ERR, "Accept failed");
            continue;
        }
        debug_log("Client connected");
        
        // Start timestamp thread only after the first client connection
        if (!timestamps_started) {
            pthread_create(&timestamp_thread, NULL, append_timestamp, NULL);
            timestamps_started = 1;
        }
        
        node = malloc(sizeof(thread_node_t));
        if (!node) {
            syslog(LOG_ERR, "Memory allocation for cleint node failed");
            close(client_fd);
            continue;
        }
        node->client_fd = client_fd;

        SLIST_INSERT_HEAD(&head, node, entries);  // store the client node thread in the list

        pthread_create(&node->thread_id, NULL, handle_client, node);
   }

   cleanup_and_exit(0);
}
