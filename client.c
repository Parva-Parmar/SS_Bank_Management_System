#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 8080

void* receiveMessages(void* socket_fd) {
    int sock = *(int*)socket_fd;
    char buffer[1024];
    while (1) {
        memset(buffer, 0, sizeof(buffer));
        int valread = read(sock, buffer, sizeof(buffer));
        if (valread <= 0) break;
        printf("%s\n", buffer);
        if (strcmp(buffer, "exit") == 0) break;
    }
    pthread_exit(NULL);
}

void showLandingMenu() {
    printf("\n===== Banking Management System =====\n");
    printf("1. Login\n");
    printf("2. Signup (Manager/Employee only)\n");
    printf("3. Exit\n");
    printf("Enter choice: ");
}

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[1024];
    char username[50], password[50];
    int choice;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation error");
        exit(EXIT_FAILURE);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection Failed");
        exit(EXIT_FAILURE);
    }

    printf("Connected to server.\n");

    pthread_t recv_thread;
    pthread_create(&recv_thread, NULL, receiveMessages, &sock);

    while (1) {
        showLandingMenu();
        scanf("%d", &choice);
        getchar(); // consume newline

        if (choice == 1) { // Login
            int role;
            printf("Enter username: ");
            scanf("%s", username);
            printf("Enter password: ");
            scanf("%s", password);

            printf("Select role:\n");
            printf("1. Customer\n");
            printf("2. Employee\n");
            printf("3. Manager\n");
            printf("Enter role number: ");
            scanf("%d", &role);

            if (role < 1 || role > 3) {
                printf("Invalid role selection.\n");
                continue;
            }

            sprintf(buffer, "LOGIN|%s|%s|%d", username, password, role);
            send(sock, buffer, strlen(buffer), 0);

        } else if (choice == 2) { // Signup
            int role;
            printf("Enter new username: ");
            scanf("%s", username);
            printf("Enter new password: ");
            scanf("%s", password);

            printf("Select role for signup:\n");
            printf("2. Employee\n");
            printf("3. Manager\n");
            printf("Enter role number: ");
            scanf("%d", &role);

            if (role != 2 && role != 3) {
                printf("Signup allowed only for Employee or Manager roles.\n");
                continue;
            }

            sprintf(buffer, "SIGNUP|%s|%s|%d", username, password, role);
            send(sock, buffer, strlen(buffer), 0);

        } else if (choice == 3) { // Exit
            strcpy(buffer, "exit");
            send(sock, buffer, strlen(buffer), 0);
            break;
        } else {
            printf("Invalid choice. Try again.\n");
        }

        memset(buffer, 0, sizeof(buffer));
        read(sock, buffer, sizeof(buffer));
        printf("Server: %s\n", buffer);
    }

    pthread_join(recv_thread, NULL);
    close(sock);
    return 0;
}
