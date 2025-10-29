#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080

void trim(char *str) {
    char *start = str;
    while (isspace((unsigned char)*start)) start++;
    char *end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end)) end--;
    *(end + 1) = '\0';
    memmove(str, start, end - start + 2);
}

void showMainMenu() {
    printf("==== Banking System ====\n");
    printf("1. Login\n");
    printf("2. Signup\n");
    printf("3. Exit\n");
    printf("Enter choice: ");
}

void showLoginMenu() {
    printf("Login as:\n");
    printf("1. Admin\n");
    printf("2. Manager\n");
    printf("3. Employee\n");
    printf("4. Customer\n");
    printf("5. Exit to Main Menu\n");
    printf("Enter choice: ");
}

void showSignupMenu() {
    printf("Signup as:\n");
    printf("1. Manager\n");
    printf("2. Employee\n");
    printf("3. Exit to Main Menu\n");
    printf("Enter choice: ");
}

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[1024] = {0};
    int main_choice, role_choice;
    char *roles[] = {"admin", "manager", "employee", "customer"};

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\nSocket creation error\n");
        return -1;
    }
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address/Address not supported\n");
        return -1;
    }
    if (connect(sock, (struct sockaddr *)&serv_addr,
                sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed\n");
        return -1;
    }

    while (1) {
        showMainMenu();
        scanf("%d", &main_choice);
        getchar();

        if (main_choice == 3) {
            send(sock, "exit", 4, 0);
            printf("Goodbye!\n");
            break;
        }

        if (main_choice == 1) {
            showLoginMenu();
            scanf("%d", &role_choice);
            getchar();
            if (role_choice == 5) continue;
            if (role_choice < 1 || role_choice > 5) {
                printf("Invalid choice.\n");
                continue;
            }

            char username[64], password[64];
            printf("Enter Username: ");
            fgets(username, sizeof(username), stdin);
            username[strcspn(username, "\n")] = 0;
            trim(username);

            printf("Enter Password: ");
            fgets(password, sizeof(password), stdin);
            password[strcspn(password, "\n")] = 0;
            trim(password);

            sprintf(buffer, "login|%s|%s|%s", roles[role_choice - 1], username, password);
            send(sock, buffer, strlen(buffer), 0);

            int valread = read(sock, buffer, 1024);
            buffer[valread] = '\0';
            printf("Server: %s\n", buffer);

            if (strstr(buffer, "Login successful.") && role_choice == 4) {
                while (1) {
                    valread = read(sock, buffer, 1024);
                    if (valread <= 0) break;
                    buffer[valread] = '\0';
                    printf("%s", buffer);

                    int cust_choice;
                    scanf("%d", &cust_choice);
                    getchar();

                    char cmd[32];
                    sprintf(cmd, "%d", cust_choice);
                    send(sock, cmd, strlen(cmd), 0);

                    if (cust_choice == 2 || cust_choice == 3) { // Deposit or Withdraw
                        valread = read(sock, buffer, 1024);
                        buffer[valread] = '\0';
                        printf("%s", buffer);

                        if (cust_choice == 2)
                            printf("Enter deposit amount: ");
                        else
                            printf("Enter withdrawal amount: ");

                        float amount;
                        scanf("%f", &amount);
                        getchar();

                        char amount_str[32];
                        snprintf(amount_str, sizeof(amount_str), "%.2f", amount);
                        send(sock, amount_str, strlen(amount_str), 0);

                        valread = read(sock, buffer, 1024);
                        buffer[valread] = '\0';
                        printf("%s", buffer);

                    } else if (cust_choice == 4) { // Transfer Funds
                        // Prompt recipient username
                        valread = read(sock, buffer, 1024);
                        buffer[valread] = '\0';
                        printf("%s", buffer);

                        char recipient[64];
                        fgets(recipient, sizeof(recipient), stdin);
                        recipient[strcspn(recipient, "\n")] = 0;
                        send(sock, recipient, strlen(recipient), 0);

                        // Prompt amount
                        valread = read(sock, buffer, 1024);
                        buffer[valread] = '\0';
                        printf("%s", buffer);

                        float transfer_amount;
                        scanf("%f", &transfer_amount);
                        getchar();
                        char amount_str[32];
                        snprintf(amount_str, sizeof(amount_str), "%.2f", transfer_amount);
                        send(sock, amount_str, strlen(amount_str), 0);

                        // Read response
                        valread = read(sock, buffer, 1024);
                        buffer[valread] = '\0';
                        printf("%s", buffer);

                    } else if (cust_choice == 5) {
                        // Wait for server prompt
                        valread = read(sock, buffer, 1024);
                        buffer[valread] = '\0';
                        printf("%s", buffer);

                        // Read loan amount from user and send to server
                        float loan_amount;
                        scanf("%f", &loan_amount);
                        getchar();

                        char loan_str[32];
                        snprintf(loan_str, sizeof(loan_str), "%.2f", loan_amount);
                        send(sock, loan_str, strlen(loan_str), 0);

                        // Read and print server response
                        valread = read(sock, buffer, 1024);
                        buffer[valread] = '\0';
                        printf("%s", buffer);
                    }
                    else if (cust_choice == 6) {  // Change Password
                        // Wait for server prompt
                        valread = read(sock, buffer, 1024);
                        buffer[valread] = '\0';
                        printf("%s", buffer);

                        // Input new password
                        char new_password[64];
                        fgets(new_password, sizeof(new_password), stdin);
                        new_password[strcspn(new_password, "\n")] = 0;  // Remove newline

                        // Send new password to server
                        send(sock, new_password, strlen(new_password), 0);

                        // Receive confirmation message and print
                        valread = read(sock, buffer, 1024);
                        buffer[valread] = '\0';
                        printf("%s", buffer);
                    }
                    else {
                        valread = read(sock, buffer, 1024);
                        if (valread <= 0) break;
                        buffer[valread] = '\0';
                        printf("%s", buffer);
                    }

                    if (cust_choice == 9 || cust_choice == 10)
                        break;
                }
            }
        } else if (main_choice == 2) {
            showSignupMenu();
            scanf("%d", &role_choice);
            getchar();
            if (role_choice == 3) continue;
            if (role_choice < 1 || role_choice > 3) {
                printf("Invalid choice.\n");
                continue;
            }

            char username[64], password[64];
            printf("Enter Username: ");
            fgets(username, sizeof(username), stdin);
            username[strcspn(username, "\n")] = 0;
            trim(username);

            printf("Enter Password: ");
            fgets(password, sizeof(password), stdin);
            password[strcspn(password, "\n")] = 0;
            trim(password);

            sprintf(buffer, "signup|%s|%s|%s", roles[role_choice], username, password);
            send(sock, buffer, strlen(buffer), 0);

            int valread = read(sock, buffer, 1024);
            buffer[valread] = '\0';
            printf("Server: %s\n", buffer);
        } else {
            printf("Invalid menu choice.\n");
        }
    }
    close(sock);
    return 0;
}
