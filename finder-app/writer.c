#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>

int main(int argc, char *argv[]) {
    openlog("writer", 
            LOG_PID,
            LOG_USER);

    if (argc != 3) {
        syslog(LOG_ERR, 
               "Error: Two arguments required (%d provided)",
               (argc-1));
        syslog(LOG_ERR,
               "Usage: %s <file-to-write> <write-string>\n",
               argv[0]);
        closelog();
        return 1;
    }

    const char *write_file = argv[1];
    const char *write_string = argv[2];

    syslog(LOG_DEBUG, "Writing %s to %s", write_string, write_file);
    FILE *file = fopen(write_file, "w");
    if (file == NULL) {
        syslog(LOG_ERR, 
               "Error: Could not create file %s - errno_msg=%s",
               write_file,
               strerror(errno));
        closelog();
        return 1;
    }

    if (fprintf(file, "%s\n", write_string) < 0) {
        syslog(LOG_ERR,
               "Error: Could not write to file %s - errno_msg=%s",
               write_file,
               strerror(errno));
        fclose(file);
        closelog();
        return 1;
    }

    if (fclose(file) != 0) {
        syslog(LOG_ERR,
               "Error: Could not close file %s - errno_msg=%s",
               write_file,
               strerror(errno));
        closelog();
        return 1;
    }

    closelog();
    return 0;
}
