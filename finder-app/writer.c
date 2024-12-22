#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syslog.h>


void create_directory(const char *path) {
    char temp[1024];
    snprintf(temp, sizeof(temp), "%s", path);
    char *p = temp;

    // Traverse the string and create directories along the way
    while (*p) {
        if (*p == '/') {
            *p = '\0';
            mkdir(temp, 0777); // Create directory with rwxrwxrwx permissions
            *p = '/';
        }
        p++;
    }
    mkdir(temp, 0777); // Create the final directory
}

int main(int argc, char *argv[]) {

    // Initialize syslog
    openlog("writer", LOG_PID | LOG_CONS, LOG_USER);

    // Check for correct number of arguments
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <writefile> <writestr>\n", argv[0]);
        closelog();

        return 1;
    }

    const char *writefile = argv[1];
    const char *writestr = argv[2];

    // Separate the directory path from the file path
    char dir_path[1024];
    snprintf(dir_path, sizeof(dir_path), "%s", writefile);

    // Find the last '/' to extract the directory path
    char *last_slash = strrchr(dir_path, '/');
    if (last_slash != NULL) {
        *last_slash = '\0'; // Terminate the string at the last '/'
        create_directory(dir_path); // Create all required directories
    }

    // Create and write to the file
    FILE *file = fopen(writefile, "w");
    if (file == NULL) {
        syslog(LOG_ERR, "Error creating file %s: %s", writefile, strerror(errno));
        fprintf(stderr, "Error creating file %s: %s\n", writefile, strerror(errno));

        closelog();

        return 1;
    }
    syslog(LOG_DEBUG, "Writing %s to %s", writestr, writefile);
    if (fprintf(file, "%s\n", writestr) < 0) {

        syslog(LOG_ERR, "Error writing to file %s", writefile);


        fprintf(stderr, "Error writing to file %s\n", writefile);
        fclose(file);
        closelog();

        return 1;
    }

    fclose(file);
    printf("File %s created successfully with content: %s\n", writefile, writestr);
    closelog();

    return 0;
}

