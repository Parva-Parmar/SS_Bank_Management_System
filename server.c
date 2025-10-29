#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>

#include "customer.h"

#define PORT 8080
#define SERVER_LOG_FILE "server.log"

pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

void trim(char *str) {
    char *start = str;
    while (isspace((unsigned char)*start)) start++;
    char *end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end)) end--;
    *(end + 1) = '\0';
    memmove(str, start, end - start + 2);
}

void write_server_log(const char *client_ip, const char *request, const char *response) {
    pthread_mutex_lock(&log_mutex);
    FILE *fp = fopen(SERVER_LOG_FILE, "a");
    if (!fp) {
        perror("Failed to open server log file");
        pthread_mutex_unlock(&log_mutex);
        return;
    }

    time_t now = time(NULL);
    char *timestamp = ctime(&now);
    timestamp[strlen(timestamp) - 1] = '\0';

    fprintf(fp, "[%s] Client: %s\nRequest: %s\nResponse: %s\n\n", timestamp, client_ip, request, response);
    fclose(fp);
    pthread_mutex_unlock(&log_mutex);
}

void handle_client(int new_socket) {
    char buffer[1024] = {0};
    char response[1024] = {0};
    char client_ip[INET_ADDRSTRLEN] = "Unknown";

    while (1) {
        int valread = read(new_socket, buffer, 1024);
        if (valread <= 0) break;
        buffer[valread] = '\0';

        if (strncmp(buffer, "exit", 4) == 0) break;

        char action[16], role[64], username[64], password[64];
        sscanf(buffer, "%[^|]|%[^|]|%[^|]|%[^|]", action, role, username, password);

        trim(action);
        trim(role);
        trim(username);
        trim(password);

        FILE *fp = NULL;

        if (strcmp(action, "login") == 0) {
            if (strcmp(role, "customer") == 0) {
                if (validate_customer_login(username, password)) {
                    strcpy(response, "Login successful.\n");
                    send(new_socket, response, strlen(response), 0);
                    write_server_log(client_ip, buffer, response);

                    char cust_menu[] =
                        "Customer Menu:\n"
                        "1. View Account Balance\n"
                        "2. Deposit Money\n"
                        "3. Withdraw Money\n"
                        "4. Transfer Funds\n"
                        "5. Apply for a Loan\n"
                        "6. Change Password\n"
                        "7. Add Feedback\n"
                        "8. View Transaction History\n"
                        "9. Logout\n"
                        "10. Exit\n"
                        "Enter choice: ";

                    while (1) {
                        send(new_socket, cust_menu, strlen(cust_menu), 0);
                        valread = read(new_socket, buffer, sizeof(buffer) - 1);
                        if (valread <= 0) break;
                        buffer[valread] = '\0';
                        int choice = atoi(buffer);

                        switch (choice) {
                        case 1: {
                            float balance = get_account_balance(username);
                            if (balance < 0)
                                snprintf(response, sizeof(response), "Error retrieving balance.\n");
                            else
                                snprintf(response, sizeof(response), "Your account balance is: %.2f\n", balance);
                            send(new_socket, response, strlen(response), 0);
                            write_server_log(client_ip, "View Account Balance", response);
                            break;
                        }
                        case 2: {
                            strcpy(response, "Enter deposit amount: ");
                            send(new_socket, response, strlen(response), 0);
                            valread = read(new_socket, buffer, sizeof(buffer) - 1);
                            if (valread <= 0) break;
                            buffer[valread] = '\0';

                            float amount = atof(buffer);
                            bool success = deposit_money(username, amount);
                            if (success)
                                strcpy(response, "Deposit successful.\n");
                            else
                                strcpy(response, "Deposit failed.\n");
                            send(new_socket, response, strlen(response), 0);
                            write_server_log(client_ip, "Deposit Money", response);
                            break;
                        }
                        case 9:
                        case 10:
                            strcpy(response, "Logging out...\n");
                            send(new_socket, response, strlen(response), 0);
                            write_server_log(client_ip, "Logout or Exit", response);
                            return;
                        default:
                            strcpy(response, "Functionality not yet implemented.\n");
                            send(new_socket, response, strlen(response), 0);
                            write_server_log(client_ip, "Other Customer Menu Choice", response);
                            break;
                        }
                    }
                } else {
                    strcpy(response, "Login failed.\n");
                    send(new_socket, response, strlen(response), 0);
                    write_server_log(client_ip, buffer, response);
                }
            } else {
                fp = fopen("users.txt", "a+");
                if (!fp) {
                    strcpy(response, "Server error.\n");
                    send(new_socket, response, strlen(response), 0);
                    write_server_log(client_ip, buffer, response);
                    continue;
                }
                int found = 0;
                char line[256];
                rewind(fp);
                while (fgets(line, sizeof(line), fp)) {
                    char utype[16], uname[64], upwd[64];
                    sscanf(line, "%[^|]|%[^|]|%[^|]", utype, uname, upwd);
                    trim(utype);
                    trim(uname);
                    trim(upwd);
                    if (strcmp(utype, role) == 0 && strcmp(uname, username) == 0 &&
                        strcmp(upwd, password) == 0) {
                        found = 1;
                        break;
                    }
                }
                fclose(fp);
                if (found)
                    strcpy(response, "Login successful.\n");
                else
                    strcpy(response, "Login failed.\n");
                send(new_socket, response, strlen(response), 0);
                write_server_log(client_ip, buffer, response);
            }
        } else if (strcmp(action, "signup") == 0) {
            fp = fopen("users.txt", "a+");
            if (!fp) {
                strcpy(response, "Server error.\n");
                send(new_socket, response, strlen(response), 0);
                write_server_log(client_ip, buffer, response);
                continue;
            }
            fprintf(fp, "%s|%s|%s\n", role, username, password);
            fclose(fp);
            strcpy(response, "Signup successful.\n");
            send(new_socket, response, strlen(response), 0);
            write_server_log(client_ip, buffer, response);
        } else {
            strcpy(response, "Invalid request.\n");
            send(new_socket, response, strlen(response), 0);
            write_server_log(client_ip, buffer, response);
        }
        memset(buffer, 0, sizeof(buffer));
    }
    close(new_socket);
}

void *thread_func(void *arg) {
    int new_socket = *(int *)arg;
    handle_client(new_socket);
    free(arg);
    return NULL;
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 3) < 0) {
        perror("Listen");
        exit(EXIT_FAILURE);
    }
    printf("Server listening on port %d...\n", PORT);

    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen)) < 0) {
            perror("Accept");
            exit(EXIT_FAILURE);
        }
        int *client_sock = malloc(sizeof(int));
        *client_sock = new_socket;
        pthread_t tid;
        pthread_create(&tid, NULL, thread_func, client_sock);
        pthread_detach(tid);
    }
    return 0;
}
