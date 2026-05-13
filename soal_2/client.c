#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 9000

int main() {
    int sock;
    struct sockaddr_in server_addr;
    char buffer[4096];

    sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock < 0) {
        perror("Socket error");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, (struct sockaddr *)&server_addr,
                sizeof(server_addr)) < 0) {
        perror("Connection failed");
        return 1;
    }

    printf("Connected to server port %d\n", PORT);

    while (1) {

        memset(buffer, 0, sizeof(buffer));

        printf("db > ");
        fgets(buffer, sizeof(buffer), stdin);

        send(sock, buffer, strlen(buffer), 0);

        if (strncmp(buffer, "EXIT", 4) == 0)
            break;

        memset(buffer, 0, sizeof(buffer));

        int valread = recv(sock, buffer, sizeof(buffer), 0);

        if (valread <= 0)
            break;

        printf("%s\n", buffer);
    }

    close(sock);

    return 0;
}