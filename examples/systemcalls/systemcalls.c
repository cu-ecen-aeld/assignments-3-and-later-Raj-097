#include "systemcalls.h"
#include <stdlib.h>    // For system()
#include <sys/wait.h>  // For WIFEXITED, WEXITSTATUS
#include <unistd.h>    // For fork(), execv(), pid_t
#include <stdarg.h>    // For variable arguments (va_list, va_start, etc.)
#include <stdio.h>     // For perror()


/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{

/*
 * TODO  add your code here
 *  Call the system() function with the command set in the cmd
 *   and return a boolean true if the system() call completed with success
 *   or false() if it returned a failure
*/
if (cmd == NULL) {
        return false; // Null command is an error
    }
fflush(stdout);

    int ret = system(cmd); // Call the system function with the command

    // Check the return value of system()
    if (ret == -1) {
        return false; // Error in invocation of system()
    }

    // WEXITSTATUS extracts the exit status of the command  and WIFEEXITED says if the child process exited normally
    if (WIFEXITED(ret) && WEXITSTATUS(ret) == 0) {
        return true; // Command executed successfully
    }

    return false; // Command failed

}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/

bool do_exec(int count, ...)
{
    va_list args;      // used to access the arguments
    va_start(args, count);  // macro that initializs args
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
 
/*
 * TODO:
 *   Execute a system command by calling fork, execv(),
 *   and wait instead of system (see LSP page 161).
 *   Use the command[0] as the full path to the command to execute
 *   (first argument to execv), and use the remaining arguments
 *   as second argument to the execv() command.
 *
*/

    va_end(args);
    
    // Check if the first command is an absolute path
    if (!is_absolute_path(command[0])) {
        fprintf(stderr, "Error: Command must be an absolute path: %s\n", command[0]);
        return false;
    }

    // Fork a child process
    fflush(stdout);

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork failed");
        return false;
    }

    if (pid == 0) {
        // Child process: Execute the command using execv
        if (execv(command[0], command) == -1) {
            perror("execv failed");
            return false;
        }
    } 
    else {
        // Parent process: Wait for the child process to finish
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid failed");
            return false;
        }

        // Check the exit status of the child process
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            return true;  // Command executed successfully
        } 
        else {
            return false;  // Command failed (non-zero exit status)
        }
    }

    return false; // Default return if execution failed (shouldn't reach here)
}

/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    command[count] = command[count];


/*
 * TODO
 *   Call execv, but first using https://stackoverflow.com/a/13784315/1446624 as a refernce,
 *   redirect standard out to a file specified by outputfile.
 *   The rest of the behaviour is same as do_exec()
 *
*/
    va_end(args);
// Check if the first command is an absolute path
    if (!is_absolute_path(command[0])) {
        fprintf(stderr, "Error: Command must be an absolute path: %s\n", command[0]);
        return false;
    }

    // Fork a child process
    fflush(stdout);

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork failed");
        return false;
    }

    if (pid == 0) {
        // Child process: Redirect stdout to the output file
        FILE *output = freopen(outputfile, "w", stdout);
        if (output == NULL) {
            perror("freopen failed");
            return false;
        }

        // Execute the command using execv
        if (execv(command[0], command) == -1) {
            perror("execv failed");
            return false;
        }
    } else {
        // Parent process: Wait for the child process to finish
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid failed");
            return false;
        }

        // Check the exit status of the child process
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            return true;  // Command executed successfully
        } else {
            return false;  // Command failed (non-zero exit status)
        }
    }

    return false; // Default return if execution failed (shouldn't reach here)
    return true;
}

bool is_absolute_path(const char *path) {
    return path[0] == '/';  // Absolute paths start with '/'
}
