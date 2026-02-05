#include "systemcalls.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdarg.h>
#include <fcntl.h> 

/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{
    int retVal = system(cmd);
    return (retVal == 0 ? true : false);
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
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL; // according to man execv(3), must have NULL termination!
    va_end(args);

    // the PID of the child process is returned in the parent, and 0 is returned in the child
    pid_t pid = fork();
    if (pid == -1) { return false; }
    
    // child process behavior - execv() does not have a return value if successful
    if (pid == 0) {
        execv(command[0], command);
        _exit(EXIT_FAILURE);
    }

    // parent process behavior : Child PID on success, -1 on error, 0 for WNOHANG
    int status;
    pid_t wait_result = waitpid(pid, &status, 0);
    if (wait_result == -1) { return false; }

    if (WIFEXITED(status)) {
        return (WEXITSTATUS(status) == 0);
    }

    return false;
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
    va_end(args);

    pid_t pid = fork();
    if (pid == -1) { return false; }

    // child process behavior - execv() does not have a return value if successful
    if (pid == 0) {
        int fd = open(outputfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd == -1) { _exit(EXIT_FAILURE); }
        
        if (dup2(fd, STDOUT_FILENO) == -1) {
            close(fd);
            _exit(EXIT_FAILURE);
        }
        
        close(fd);
        execv(command[0], command);
        _exit(EXIT_FAILURE);
    }

    // parent process behavior : Child PID on success, -1 on error, 0 for WNOHANG
    int status;
    pid_t wait_result = waitpid(pid, &status, 0);
    if (wait_result == -1) { return false; }

    // Check if child exited normally and with exit code 0
    if (WIFEXITED(status)) {
        return (WEXITSTATUS(status) == 0);
    }

    return false;
}
