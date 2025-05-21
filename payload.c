#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

void daemonize(){
    pid_t pid;

    if( (pid = fork()) < 0 ) exit( EXIT_FAILURE );
    
    else if( pid > 0 )
        exit(0);

    if( (pid = fork()) < 0 ) exit( EXIT_FAILURE );

    else if( pid > 0 )
        exit( 0 );

    if (setsid() < 0)  exit( EXIT_FAILURE );

    for (int i = 0; i < sysconf(_SC_OPEN_MAX); i++) {
        close(i);
    }
    
    int fd = open("/dev/null", O_RDWR);
    dup2(fd, STDIN_FILENO);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    if (fd > STDERR_FILENO) close(fd);

    if( chdir( "/" ) < 0 ) exit( EXIT_FAILURE );
}

int main(){

    daemonize();
    while (1) {
        sleep(1);
    }
    return (EXIT_SUCCESS);
}