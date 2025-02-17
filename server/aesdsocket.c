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

#define PORT "9000" // Port number to listen on as a string
#define BACKLOG 10   // Maximum number of pending connections in the queue
#define FILE_PATH "/var/tmp/aesdsocketdata" // Path to the file to store received data

int server_fd = -1; // Server socket file descriptor
int client_fd = -1; // Client socket file descriptor
int file_fd = -1;   // File descriptor for the file to store data

// Signal handler to clean up resources when the program is interrupted or terminated
void cleanup_and_exit() {
    syslog(LOG_INFO, "Caught signal, exiting");
    if (client_fd != -1) close(client_fd); // Close client socket
    if (server_fd != -1) close(server_fd); // Close server socket
    if (file_fd != -1) close(file_fd);     // Close file descriptor
    unlink(FILE_PATH);                     // Remove the temporary file
    closelog();                             // Close the syslog connection
    exit(0);                                // Exit the program
}

// Function to set up signal handlers for SIGINT and SIGTERM
void setup_signal_handlers() {
    struct sigaction sa;
/* struct sigaction {
    void     (*sa_handler)(int);    // Function to handle the signal
    void     (*sa_sigaction)(int, siginfo_t *, void *); // Alternative handler
    sigset_t sa_mask;               // Signals to block during execution of handler
    int      sa_flags;              // Flags to modify behavior
    void     (*sa_restorer)(void);  // Unused, should be set to NULL
};
*/
    sa.sa_handler = cleanup_and_exit; // Set the handler to cleanup_and_exit
    sigemptyset(&sa.sa_mask);         // No additional signals to block
    sa.sa_flags = 0;                  // Default behavior
    sigaction(SIGINT, &sa, NULL);     // Handle SIGINT (Ctrl+C)
    sigaction(SIGTERM, &sa, NULL);    // Handle SIGTERM (termination signal)
}

int main() {
    struct addrinfo hints, *res;
/* struct addrinfo {
    int ai_flags;           // AI_PASSIVE, AI_CANONNAME, etc.
    int ai_family;          // AF_INET (IPv4) or AF_INET6 (IPv6)
    int ai_socktype;        // SOCK_STREAM (TCP) or SOCK_DGRAM (UDP)
    int ai_protocol;        // 0 for automatic selection
    size_t ai_addrlen;      // Size of `ai_addr`
    struct sockaddr *ai_addr; // Pointer to IP address structure
    char *ai_canonname;     // Canonical hostname
    struct addrinfo *ai_next; // Pointer to next addrinfo (linked list)
}; */
    struct sockaddr_storage client_addr;
/* struct sockaddr_storage {
    sa_family_t ss_family;  // Address family (AF_INET or AF_INET6)
    char __ss_padding[128]; // Padding to fit any address type
}; */
    
    socklen_t client_addr_len;  // cleint socket address length
    
    char client_ip[INET_ADDRSTRLEN];  // Buffer to store client IP address
    char buffer[1024];                // Buffer to store received data
    ssize_t bytes_read;

    // (a) Open syslog for logging
    openlog("aesdsocket", LOG_PID, LOG_USER);
    // LOG_PID -> adds the PID to each log message
    // LOG_USER -> this log is from a user-level program

    // (b) Set up hints for getaddrinfo (to resolve server address and port)
    memset(&hints, 0, sizeof(hints)); // Clear the structure hints
    hints.ai_family = AF_INET;        // Using IPv4 addresses
    hints.ai_socktype = SOCK_STREAM;  // Using TCP sockets
    hints.ai_flags = AI_PASSIVE;      // Allow the system to choose the IP address

    // Get address information for the specified port
    if (getaddrinfo(NULL, PORT, &hints, &res) != 0) {
    // NULL -> Binds to any available IP
        syslog(LOG_ERR, "getaddrinfo failed");
        exit(EXIT_FAILURE);
    }

    // (b) Create server socket
    server_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (server_fd == -1) {
        syslog(LOG_ERR, "Socket creation failed");
        freeaddrinfo(res); // Free address info structure
        exit(EXIT_FAILURE);
    }

    // (b) Bind socket to the address and port
    if (bind(server_fd, res->ai_addr, res->ai_addrlen) == -1) {
        syslog(LOG_ERR, "Bind failed");
        close(server_fd);
        freeaddrinfo(res); // Free address info structure
        exit(EXIT_FAILURE);
    }
    freeaddrinfo(res); // No longer need address info after binding

    // (b) Listen for incoming connections
    if (listen(server_fd, BACKLOG) == -1) {
        syslog(LOG_ERR, "Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // (h) Setup signal handlers
    setup_signal_handlers();

    while (1) {
        // (c) Accept incoming client connection
        client_addr_len = sizeof(client_addr);
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_fd == -1) {
            syslog(LOG_ERR, "Accept failed");
            continue; // Skip this loop iteration and wait for the next connection
        }

        // (d) Log the accepted connection (client IP address)
        inet_ntop(client_addr.ss_family, &((struct sockaddr_in *)&client_addr)->sin_addr, client_ip, sizeof(client_ip));
        // inet_ntop() coverts the binary ip address to a human-readable string
        syslog(LOG_INFO, "Accepted connection from %s", client_ip);

        // (e) Open the file to append received data
        file_fd = open(FILE_PATH, O_RDWR | O_CREAT | O_APPEND, 0666);
        if (file_fd == -1) {
            syslog(LOG_ERR, "Failed to open file");
            close(client_fd);
            continue; // Skip this loop iteration and wait for the next client
        }

        // (e) Receive data from the client
        while ((bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0)) > 0) {
            buffer[bytes_read] = '\0';  // Null-terminate the received data
            char *newline_pos = strchr(buffer, '\n');
	    if (newline_pos) {
    	    	size_t length_to_write = newline_pos - buffer + 1; // Include '\n'
    		write(file_fd, buffer, length_to_write);
    		break;
           } else {
    		write(file_fd, buffer, bytes_read);
	   }
        }

        // (f) Handle read errors
        if (bytes_read == -1) {
            syslog(LOG_ERR, "Receive failed");
            close(client_fd);
            close(file_fd);
            continue; // Skip this loop iteration and wait for the next client
        }

        // (f) Send the file content back to the client
        lseek(file_fd, 0, SEEK_SET); // Move file pointer to the start
        while ((bytes_read = read(file_fd, buffer, sizeof(buffer))) > 0) {
    		long int total_sent = 0;
    		while (total_sent < bytes_read) {
        		long int bytes_sent = send(client_fd, buffer + total_sent, bytes_read - total_sent, 0);
        		if (bytes_sent == -1) {
            			syslog(LOG_ERR, "Send failed");
            			break; // Exit the inner loop on failure
        		}
        		total_sent += bytes_sent;
    		}
	}


        // (g) Log the closed connection
        syslog(LOG_INFO, "Closed connection from %s", client_ip);
        close(client_fd); // Close the client socket
        close(file_fd);   // Close the file descriptor
        client_fd = -1;   // Reset client socket file descriptor
        file_fd = -1;     // Reset file descriptor
    }

    cleanup_and_exit(0); // Cleanup and exit when the server stops (should not reach here in this loop)
}

