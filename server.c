#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include <stdbool.h>

#define PORT 8080
#define USERS_FILE "users.txt"
#define MAX_SESSIONS 100

typedef struct {
    char username[50];
    char password[50];
    int role; // 1=Customer, 2=Employee, 3=Manager, 4=Admin
} User;

typedef struct {
    char username[50];
    bool active;
} Session;

Session sessions[MAX_SESSIONS];
pthread_mutex_t session_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;

// Initialize sessions
void initSessions() {
    for (int i = 0; i < MAX_SESSIONS; i++) {
        sessions[i].active = false;
    }
}

bool isUserLoggedIn(char* username) {
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (sessions[i].active && strcmp(sessions[i].username, username) == 0) {
            return true;
        }
    }
    return false;
}

void addSession(char* username) {
    pthread_mutex_lock(&session_mutex);
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (!sessions[i].active) {
            strcpy(sessions[i].username, username);
            sessions[i].active = true;
            break;
        }
    }
    pthread_mutex_unlock(&session_mutex);
}

void removeSession(char* username) {
    pthread_mutex_lock(&session_mutex);
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (sessions[i].active && strcmp(sessions[i].username, username) == 0) {
            sessions[i].active = false;
            break;
        }
    }
    pthread_mutex_unlock(&session_mutex);
}

int checkLogin(char* username, char* password, int* role) {
    FILE* file = fopen(USERS_FILE, "r");
    if (!file) return 0;

    char file_username[50], file_password[50];
    int file_role;
    while (fscanf(file, "%s %s %d", file_username, file_password, &file_role) != EOF) {
        if (strcmp(username, file_username) == 0 &&
            strcmp(password, file_password) == 0) {
            *role = file_role;
            fclose(file);
            return 1;
        }
    }
    fclose(file);
    return 0;
}

int addUser(char* username, char* password, int role) {
    FILE* file = fopen(USERS_FILE, "a+");
    if (!file) return 0;

    char file_username[50], file_password[50];
    int file_role;
    rewind(file);
    while (fscanf(file, "%s %s %d", file_username, file_password, &file_role) != EOF) {
        if (strcmp(username, file_username) == 0) {
            fclose(file);
            return 0;
        }
    }

    fprintf(file, "%s %s %d\n", username, password, role);
    fclose(file);
    return 1;
}

void* handleClient(void* arg) {
    int client_socket = *(int*)arg;
    char buffer[1024];
    char loggedUser[50] = "";

    printf("[SERVER] Client connected.\n");

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        int valread = read(client_socket, buffer, sizeof(buffer));
        if (valread <= 0) break;

        if (strncmp(buffer, "LOGIN|", 6) == 0) {
            char username[50], password[50];
            int role = 0, input_role;
            sscanf(buffer, "LOGIN|%[^|]|%[^|]|%d", username, password, &input_role);

            pthread_mutex_lock(&session_mutex);
            if (isUserLoggedIn(username)) {
                send(client_socket, "LOGIN_FAIL: Already logged in", 30, 0);
                printf("[SERVER] Login failed — user '%s' already logged in.\n", username);
                pthread_mutex_unlock(&session_mutex);
                continue;
            }
            pthread_mutex_unlock(&session_mutex);

            pthread_mutex_lock(&file_mutex);
            int success = checkLogin(username, password, &role);
            pthread_mutex_unlock(&file_mutex);

            if (success && role == input_role) {
                addSession(username);
                strcpy(loggedUser, username);
                sprintf(buffer, "LOGIN_SUCCESS|%d", role);
                send(client_socket, buffer, strlen(buffer), 0);
                printf("[SERVER] User '%s' logged in as role %d.\n", username, role);
            } else {
                send(client_socket, "LOGIN_FAIL", 10, 0);
                printf("[SERVER] Login failed for username '%s'.\n", username);
            }

        } else if (strncmp(buffer, "SIGNUP|", 7) == 0) {
            char username[50], password[50];
            int role;
            sscanf(buffer, "SIGNUP|%[^|]|%[^|]|%d", username, password, &role);

            // Only allow Employee(2) or Manager(3) signup
            if (role != 2 && role != 3) {
                send(client_socket, "SIGNUP_FAIL: Only Manager or Employee allowed", 45, 0);
                printf("[SERVER] Signup failed — role %d not allowed for username '%s'.\n", role, username);
                continue;
            }

            pthread_mutex_lock(&file_mutex);
            int success = addUser(username, password, role);
            pthread_mutex_unlock(&file_mutex);

            if (success) {
                send(client_socket, "SIGNUP_SUCCESS", 14, 0);
                printf("[SERVER] New user '%s' signed up with role %d.\n", username, role);
            } else {
                send(client_socket, "SIGNUP_FAIL: Username already exists", 38, 0);
                printf("[SERVER] Signup failed — username '%s' already exists.\n", username);
            }

        } else if (strcmp(buffer, "exit") == 0) {
            send(client_socket, "exit", 4, 0);
            if (strlen(loggedUser) > 0) removeSession(loggedUser);
            printf("[SERVER] Client disconnected.\n");
            break;
        } else {
            send(client_socket, "INVALID_COMMAND", 15, 0);
            printf("[SERVER] Received invalid command: %s\n", buffer);
        }
    }

    if (strlen(loggedUser) > 0) removeSession(loggedUser);

    close(client_socket);
    pthread_exit(NULL);
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    pthread_t tid;

    initSessions();

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 5) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("[SERVER] Listening on port %d...\n", PORT);

    while (1) {
        new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
        if (new_socket < 0) {
            perror("[SERVER] Accept failed");
            continue;
        }
        pthread_create(&tid, NULL, handleClient, (void*)&new_socket);
    }

    close(server_fd);
    return 0;
}
