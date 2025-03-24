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
    
    unlink(FILE_PATH); // Remove the temporary file upon exit
    pthread_mutex_destroy(&file_mutex); // Destroy the mutex to prevent memory leaks
    closelog(); // Close syslog
    exit(0);
}

void *append_timestamp(void *arg) {
    (void)arg;  // Mark argument as unused

    sleep(10);  // Ensure the first timestamp is delayed by 10 seconds

    while (!shutdown_flag) {
        // Get the current time
        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        char time_str[100];

        strftime(time_str, sizeof(time_str), "timestamp: %a, %d %b %Y %H:%M:%S %z\n", tm_info);

        // Lock the file to avoid race conditions
        pthread_mutex_lock(&file_mutex);
        FILE *file = fopen(FILE_PATH, "a");
        if (file) {
            fputs(time_str, file);
            fclose(file);
        } else {
            syslog(LOG_ERR, "Failed to open file for timestamp writing");
        }
        pthread_mutex_unlock(&file_mutex);

        sleep(10);  // Wait for 10 seconds before writing the next timestamp
    }
    return NULL;
}

void *handle_client(void *arg) {
    thread_node_t *node = (thread_node_t *)arg;
    int client_fd = node->client_fd;
    char buffer[1024];
    ssize_t bytes_read;

    syslog(LOG_INFO, "Handling new client connection");

    while ((bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes_read] = '\0';  // Null-terminate received data
        syslog(LOG_INFO, "Received data: %s", buffer);

        pthread_mutex_lock(&file_mutex);
        FILE *file = fopen(FILE_PATH, "a");
        if (file) {
            fputs(buffer, file);  // Write received data to file
            fclose(file);
        } else {
            syslog(LOG_ERR, "Failed to open file for writing");
        }
        pthread_mutex_unlock(&file_mutex);
    }

    if (bytes_read == -1) {
        syslog(LOG_ERR, "Receive failed: %s", strerror(errno));
    }

    close(client_fd);
    SLIST_REMOVE(&head, node, thread_node, entries);
    free(node);
    
    pthread_exit(NULL);
}



int main(int argc, char *argv[]) {
    const char *filepath = "/var/tmp/aesdsocketdata";
    
    int daemon_mode = (argc > 1 && strcmp(argv[1], "-d") == 0);
    struct addrinfo hints, *res;
    struct sockaddr_storage client_addr;
    socklen_t client_addr_len;
    thread_node_t *node;

    openlog("aesdsocket", LOG_PID, LOG_USER);
    struct sigaction sa = { .sa_handler = cleanup_and_exit };
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    
    if (getaddrinfo(NULL, PORT, &hints, &res) != 0) {
        syslog(LOG_ERR, "getaddrinfo failed");
        exit(EXIT_FAILURE);
    }

    server_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (server_fd == -1) {
        syslog(LOG_ERR, "Socket creation failed");
        freeaddrinfo(res);
        exit(EXIT_FAILURE);
    }
    
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
    
    if (listen(server_fd, BACKLOG) == -1) {
        syslog(LOG_ERR, "Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    
    int timestamps_started = 0;
    while (!shutdown_flag) {
        client_addr_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_fd == -1) {
            if (shutdown_flag) break;
            syslog(LOG_ERR, "Accept failed");
            continue;
        }

        if (!timestamps_started) {
            pthread_create(&timestamp_thread, NULL, append_timestamp, NULL);
            timestamps_started = 1;
        }
        //  Allocate memory for a new thread node
    thread_node_t *node = malloc(sizeof(thread_node_t));
    if (!node) {
        syslog(LOG_ERR, "Memory allocation failed");
        close(client_fd);
        continue;
    }

    node->client_fd = client_fd;

    //  Add this thread to the list
    SLIST_INSERT_HEAD(&head, node, entries);

    //  Create a thread to handle the client
    pthread_create(&node->thread_id, NULL, handle_client, node);
    }
    cleanup_and_exit(0);
}

