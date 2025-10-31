#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080

void trim(char *str)
{
    char *start = str;
    while (isspace((unsigned char)*start))
        start++;
    char *end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end))
        end--;
    *(end + 1) = '\0';
    memmove(str, start, end - start + 2);
}

void read_full_prompt(int sock, char *buffer, size_t buff_size, const char *prompt_end)
{
    size_t total_read = 0;
    ssize_t bytes_read;
    while (total_read < buff_size - 1)
    {
        bytes_read = read(sock, buffer + total_read, buff_size - 1 - total_read);
        if (bytes_read <= 0)
        {
            break; // Connection closed or error
        }
        total_read += bytes_read;
        buffer[total_read] = '\0';
        if (strstr(buffer, prompt_end) != NULL)
        {
            break; // Found expected prompt ending
        }
    }
}

int read_int_from_stdin()
{
    char buf[16];
    if (!fgets(buf, sizeof(buf), stdin))
        return -1;
    return atoi(buf);
}
void showMainMenu()
{
    printf("==== Banking System ====\n");
    printf("1. Login\n");
    printf("2. Signup\n");
    printf("3. Exit\n");
    printf("Enter choice: ");
}

void showLoginMenu()
{
    printf("Login as:\n");
    printf("1. Admin\n");
    printf("2. Manager\n");
    printf("3. Employee\n");
    printf("4. Customer\n");
    printf("5. Exit to Main Menu\n");
    printf("Enter choice: ");
}

void showSignupMenu()
{
    printf("Signup as:\n");
    printf("1. Manager\n");
    printf("2. Employee\n");
    printf("3. Exit to Main Menu\n");
    printf("Enter choice: ");
}

int main()
{
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[1024] = {0};
    int main_choice, role_choice;
    char *roles[] = {"admin", "manager", "employee", "customer"};

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\nSocket creation error\n");
        return -1;
    }
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0)
    {
        printf("\nInvalid address/Address not supported\n");
        return -1;
    }
    if (connect(sock, (struct sockaddr *)&serv_addr,
                sizeof(serv_addr)) < 0)
    {
        printf("\nConnection Failed\n");
        return -1;
    }

    while (1)
    {
        showMainMenu();
        main_choice = read_int_from_stdin();
        if (main_choice == -1)
            break;

        if (main_choice == 3)
        {
            send(sock, "exit", 4, 0);
            printf("Goodbye!\n");
            break;
        }

        if (main_choice == 1)
        {
            showLoginMenu();
            role_choice = read_int_from_stdin();
            if (role_choice == 5)
                continue;
            if (role_choice < 1 || role_choice > 5)
            {
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

            if (strstr(buffer, "Login successful.") && role_choice == 4)
            {
                while (1)
                {
                    valread = read(sock, buffer, 1024);
                    if (valread <= 0)
                        break;
                    buffer[valread] = '\0';
                    printf("%s", buffer);

                    int cust_choice;
                    scanf("%d", &cust_choice);
                    getchar();

                    char cmd[32];
                    sprintf(cmd, "%d", cust_choice);
                    send(sock, cmd, strlen(cmd), 0);

                    if (cust_choice == 2 || cust_choice == 3)
                    { // Deposit or Withdraw
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
                    }
                    else if (cust_choice == 4)
                    { // Transfer Funds
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
                    }
                    else if (cust_choice == 5)
                    {
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
                    else if (cust_choice == 6)
                    { // Change Password
                        // Wait for server prompt
                        valread = read(sock, buffer, 1024);
                        buffer[valread] = '\0';
                        printf("%s", buffer);

                        // Input new password
                        char new_password[64];
                        fgets(new_password, sizeof(new_password), stdin);
                        new_password[strcspn(new_password, "\n")] = 0; // Remove newline

                        // Send new password to server
                        send(sock, new_password, strlen(new_password), 0);

                        // Receive confirmation message and print
                        valread = read(sock, buffer, 1024);
                        buffer[valread] = '\0';
                        printf("%s", buffer);
                    }
                    else if (cust_choice == 7)
                    { // Add Feedback
                        // Wait for server prompt
                        valread = read(sock, buffer, 1024);
                        buffer[valread] = '\0';
                        printf("%s", buffer);

                        // Input feedback (one line)
                        char feedback[256];
                        fgets(feedback, sizeof(feedback), stdin);
                        feedback[strcspn(feedback, "\n")] = 0; // Remove newline

                        // Send feedback to server
                        send(sock, feedback, strlen(feedback), 0);

                        // Receive confirmation and display
                        valread = read(sock, buffer, 1024);
                        buffer[valread] = '\0';
                        printf("%s", buffer);
                    }
                    else if (cust_choice == 8)
                    { // View Transaction History
                        int valread = read(sock, buffer, sizeof(buffer) - 1);
                        if (valread <= 0)
                        {
                            printf("Failed to read transaction history.\n");
                        }
                        else
                        {
                            buffer[valread] = '\0';
                            printf("%s", buffer);
                        }
                    }
                    else
                    {
                        valread = read(sock, buffer, 1024);
                        if (valread <= 0)
                            break;
                        buffer[valread] = '\0';
                        printf("%s", buffer);
                    }

                    if (cust_choice == 9 || cust_choice == 10)
                        break;
                }
            }
            else if (strstr(buffer, "Login successful.") && role_choice == 3)
            {
                char buffer[1024];
                char input[256];
                int valread;

                while (1)
                {
                    // Receive and print menu or prompt message from server
                    read_full_prompt(sock, buffer, sizeof(buffer), "Enter choice:");
                    printf("%s", buffer);
                    // Read user input (menu choice)
                    if (fgets(input, sizeof(input), stdin) == NULL)
                        break;
                    input[strcspn(input, "\n")] = 0; // Remove newline
                    send(sock, input, strlen(input), 0);

                    int choice = atoi(input);

                    if (choice == 1)
                    {
                        char buffer[1024];
                        char input[256];
                        int valread;

                        // Receive prompt for username
                        valread = read(sock, buffer, sizeof(buffer) - 1);
                        if (valread <= 0)
                            break;
                        buffer[valread] = '\0';
                        printf("%s", buffer);

                        // Read username from user input
                        if (!fgets(input, sizeof(input), stdin))
                            break;
                        input[strcspn(input, "\n")] = 0; // Remove newline
                        send(sock, input, strlen(input), 0);

                        // Receive prompt for password
                        valread = read(sock, buffer, sizeof(buffer) - 1);
                        if (valread <= 0)
                            break;
                        buffer[valread] = '\0';
                        printf("%s", buffer);

                        // Read password from user input
                        if (!fgets(input, sizeof(input), stdin))
                            break;
                        input[strcspn(input, "\n")] = 0;
                        send(sock, input, strlen(input), 0);

                        // Add new - Receive prompt for mobile number
                        valread = read(sock, buffer, sizeof(buffer) - 1);
                        if (valread <= 0)
                            break;
                        buffer[valread] = '\0';
                        printf("%s", buffer);

                        // Read mobile number from user input
                        if (!fgets(input, sizeof(input), stdin))
                            break;
                        input[strcspn(input, "\n")] = 0;
                        send(sock, input, strlen(input), 0);

                        // Receive operation result message from server
                        valread = read(sock, buffer, sizeof(buffer) - 1);
                        if (valread <= 0)
                            break;
                        buffer[valread] = '\0';
                        printf("%s", buffer);

                        // Receive the employee menu prompt again
                        valread = read(sock, buffer, sizeof(buffer) - 1);
                        if (valread <= 0)
                            break;
                        buffer[valread] = '\0';
                        printf("%s", buffer);
                    }
                    else if (choice == 2)
                    { // Another operation, e.g., Modify Customer Details
                        char buffer[1024];
                        char input[128];
                        int valread;

                        // 1. Receive prompt for account number
                        valread = read(sock, buffer, sizeof(buffer) - 1);
                        if (valread <= 0)
                            break;
                        buffer[valread] = '\0';
                        printf("%s", buffer);

                        // Send account number input
                        if (!fgets(input, sizeof(input), stdin))
                            break;
                        input[strcspn(input, "\n")] = 0;
                        send(sock, input, strlen(input), 0);

                        // 2. Receive prompt to update mobile?
                        valread = read(sock, buffer, sizeof(buffer) - 1);
                        if (valread <= 0)
                            break;
                        buffer[valread] = '\0';
                        printf("%s", buffer);

                        // Send yes/no for updating mobile
                        if (!fgets(input, sizeof(input), stdin))
                            break;
                        input[strcspn(input, "\n")] = 0;
                        send(sock, input, strlen(input), 0);

                        if (strcasecmp(input, "yes") == 0 || strcasecmp(input, "y") == 0)
                        {
                            // Receive prompt for new mobile number
                            valread = read(sock, buffer, sizeof(buffer) - 1);
                            if (valread <= 0)
                                break;
                            buffer[valread] = '\0';
                            printf("%s", buffer);

                            if (!fgets(input, sizeof(input), stdin))
                                break;
                            input[strcspn(input, "\n")] = 0;
                            send(sock, input, strlen(input), 0);
                        }

                        // 3. Receive prompt to update password?
                        valread = read(sock, buffer, sizeof(buffer) - 1);
                        if (valread <= 0)
                            break;
                        buffer[valread] = '\0';
                        printf("%s", buffer);

                        // Send yes/no for updating password
                        if (!fgets(input, sizeof(input), stdin))
                            break;
                        input[strcspn(input, "\n")] = 0;
                        send(sock, input, strlen(input), 0);

                        if (strcasecmp(input, "yes") == 0 || strcasecmp(input, "y") == 0)
                        {
                            // Receive prompt for new password
                            valread = read(sock, buffer, sizeof(buffer) - 1);
                            if (valread <= 0)
                                break;
                            buffer[valread] = '\0';
                            printf("%s", buffer);

                            if (!fgets(input, sizeof(input), stdin))
                                break;
                            input[strcspn(input, "\n")] = 0;
                            send(sock, input, strlen(input), 0);
                        }

                        // 4. Receive operation confirmation
                        valread = read(sock, buffer, sizeof(buffer) - 1);
                        if (valread <= 0)
                            break;
                        buffer[valread] = '\0';
                        printf("%s", buffer);

                        // 5. Receive employee menu prompt again
                        valread = read(sock, buffer, sizeof(buffer) - 1);
                        if (valread <= 0)
                            break;
                        buffer[valread] = '\0';
                        printf("%s", buffer);
                    }
                    else if (choice == 3 || choice == 4 || choice == 5)
                    {
                        char buffer[4096]; // Adjust size for expected loan list info
                        // Read full loan list and prompt from server
                        read_full_prompt(sock, buffer, sizeof(buffer), "Enter loan ID to process or 'back' to return:");
                        printf("%s", buffer);

                        // Get user input
                        char input[128];
                        if (!fgets(input, sizeof(input), stdin))
                        {
                            perror("Input error");
                            break;
                        }
                        input[strcspn(input, "\n")] = 0; // Trim newline

                        // Send loan ID or "back" to server
                        send(sock, input, strlen(input), 0);

                        // Read server response/confirmation
                        read_full_prompt(sock, buffer, sizeof(buffer), "Employee Menu:");
                        printf("%s", buffer);
                    }
                    else if (choice == 6)
                    {
                        // view_assigned_loan_applications input/output or menu display
                    }
                    else if (choice == 7)
                    {
                        // view_customer_transactions input/output
                    }
                    else if (choice == 8)
                    { // Change Password example
                        valread = read(sock, buffer, sizeof(buffer) - 1);
                        if (valread <= 0)
                            break;
                        buffer[valread] = '\0';
                        printf("%s", buffer);

                        if (fgets(input, sizeof(input), stdin) == NULL)
                            break;
                        input[strcspn(input, "\n")] = 0;
                        send(sock, input, strlen(input), 0);

                        valread = read(sock, buffer, sizeof(buffer) - 1);
                        if (valread <= 0)
                            break;
                        buffer[valread] = '\0';
                        printf("%s", buffer);
                    }
                    else if (choice == 9)
                    { // Logout
                        printf("Logging out...\n");
                        break;
                    }
                    else
                    {
                        // For unrecognized choices, receive any message and print
                        valread = read(sock, buffer, sizeof(buffer) - 1);
                        if (valread <= 0)
                            break;
                        buffer[valread] = '\0';
                        printf("%s", buffer);
                    }
                }
            }
            else if (strstr(buffer, "Login successful.") && role_choice == 1)
            {
                while (1)
                {
                    valread = read(sock, buffer, 1024);
                    if (valread <= 0)
                        break;
                    buffer[valread] = '\0';
                    printf("%s", buffer);

                    int admin_choice;
                    scanf("%d", &admin_choice);
                    getchar();

                    char cmd[32];
                    sprintf(cmd, "%d", admin_choice);
                    send(sock, cmd, strlen(cmd), 0);

                    if (admin_choice == 1) // Add New Employee
                    {
                        // Receive prompt for role
                        valread = read(sock, buffer, 1024);
                        buffer[valread] = '\0';
                        printf("%s", buffer);
                        char role[16];
                        fgets(role, sizeof(role), stdin);
                        role[strcspn(role, "\n")] = 0;
                        send(sock, role, strlen(role), 0);

                        // Receive prompt for username
                        valread = read(sock, buffer, 1024);
                        buffer[valread] = '\0';
                        printf("%s", buffer);
                        char username[64];
                        fgets(username, sizeof(username), stdin);
                        username[strcspn(username, "\n")] = 0;
                        send(sock, username, strlen(username), 0);

                        // Receive prompt for password
                        valread = read(sock, buffer, 1024);
                        buffer[valread] = '\0';
                        printf("%s", buffer);
                        char password[64];
                        fgets(password, sizeof(password), stdin);
                        password[strcspn(password, "\n")] = 0;
                        send(sock, password, strlen(password), 0);

                        // Receive prompt for mobile number
                        valread = read(sock, buffer, 1024);
                        buffer[valread] = '\0';
                        printf("%s", buffer);
                        char mobile[32];
                        fgets(mobile, sizeof(mobile), stdin);
                        mobile[strcspn(mobile, "\n")] = 0;
                        send(sock, mobile, strlen(mobile), 0);

                        // Receive response from server
                        valread = read(sock, buffer, 1024);
                        buffer[valread] = '\0';
                        printf("%s", buffer);
                    }
                    else if (admin_choice == 2) // Modify User Details
                    {
                        char buffer[1024];
                        char input[128];
                        int valread;

                        // Receive & respond to "Modify which user type?" prompt
                        valread = recv(sock, buffer, sizeof(buffer) - 1, 0);
                        if (valread <= 0)
                            break;
                        buffer[valread] = '\0';
                        printf("%s", buffer);
                        if (!fgets(input, sizeof(input), stdin))
                            break;
                        input[strcspn(input, "\n")] = 0;
                        send(sock, input, strlen(input), 0);

                        // Receive & respond to "Enter username:" prompt
                        valread = recv(sock, buffer, sizeof(buffer) - 1, 0);
                        if (valread <= 0)
                            break;
                        buffer[valread] = '\0';
                        printf("%s", buffer);
                        if (!fgets(input, sizeof(input), stdin))
                            break;
                        input[strcspn(input, "\n")] = 0;
                        send(sock, input, strlen(input), 0);

                        // Receive & respond to "Enter new password or write 'NO'..." prompt
                        valread = recv(sock, buffer, sizeof(buffer) - 1, 0);
                        if (valread <= 0)
                            break;
                        buffer[valread] = '\0';
                        printf("%s", buffer);
                        if (!fgets(input, sizeof(input), stdin))
                            break;
                        input[strcspn(input, "\n")] = 0;
                        send(sock, input, strlen(input), 0);

                        // Receive & respond to "Enter new mobile number or write 'NO'..." prompt
                        valread = recv(sock, buffer, sizeof(buffer) - 1, 0);
                        if (valread <= 0)
                            break;
                        buffer[valread] = '\0';
                        printf("%s", buffer);
                        if (!fgets(input, sizeof(input), stdin))
                            break;
                        input[strcspn(input, "\n")] = 0;
                        send(sock, input, strlen(input), 0);

                        // Receive final result message from server
                        valread = recv(sock, buffer, sizeof(buffer) - 1, 0);
                        if (valread <= 0)
                            break;
                        buffer[valread] = '\0';
                        printf("%s", buffer);
                    }
                    else if (admin_choice == 3)
                    { // Change Password
                        char buffer[1024];
                        char input_buffer[128];
                        // Receive prompt for username
                        int valread = recv(sock, buffer, sizeof(buffer) - 1, 0);
                        if (valread <= 0)
                            break;
                        buffer[valread] = '\0';
                        printf("%s", buffer); // Print server prompt

                        // Get username input from user
                        fgets(input_buffer, sizeof(input_buffer), stdin);
                        input_buffer[strcspn(input_buffer, "\n")] = 0; // Remove newline
                        send(sock, input_buffer, strlen(input_buffer), 0);

                        // Receive prompt for new password
                        valread = recv(sock, buffer, sizeof(buffer) - 1, 0);
                        if (valread <= 0)
                            break;
                        buffer[valread] = '\0';
                        printf("%s", buffer);

                        // Get new password from user
                        fgets(input_buffer, sizeof(input_buffer), stdin);
                        input_buffer[strcspn(input_buffer, "\n")] = 0;
                        send(sock, input_buffer, strlen(input_buffer), 0);

                        // Receive server response
                        valread = recv(sock, buffer, sizeof(buffer) - 1, 0);
                        if (valread <= 0)
                            break;
                        buffer[valread] = '\0';
                        printf("%s", buffer);
                    }

                    else if (admin_choice == 4)
                    {
                        char buffer[1024];
                        char input[128];
                        int valread;

                        // Receive prompt from server: "Enter username to change role: "
                        valread = recv(sock, buffer, sizeof(buffer) - 1, 0);
                        if (valread <= 0)
                        {
                            printf("Server closed connection or error.\n");
                            break;
                        }
                        buffer[valread] = '\0';
                        printf("%s", buffer);

                        // Read username input from admin
                        if (!fgets(input, sizeof(input), stdin))
                        {
                            printf("Input error.\n");
                            break;
                        }
                        input[strcspn(input, "\n")] = 0; // Remove trailing newline

                        // Send username to server
                        send(sock, input, strlen(input), 0);

                        // Receive response from server (success or failure message)
                        valread = recv(sock, buffer, sizeof(buffer) - 1, 0);
                        if (valread <= 0)
                        {
                            printf("Server closed connection or error.\n");
                            break;
                        }
                        buffer[valread] = '\0';

                        // Display server response
                        printf("%s", buffer);
                    }
                    else if (admin_choice == 5) // Logout
                    {
                        printf("Logging out...\n");
                        break;
                    }
                    else
                    {
                        valread = read(sock, buffer, 1024);
                        if (valread <= 0)
                            break;
                        buffer[valread] = '\0';
                        printf("%s", buffer);
                    }
                }
            }
        }
        else if (main_choice == 2)
        {
            showSignupMenu();
            int role_choice;
            if (scanf("%d", &role_choice) != 1)
            {
                while (getchar() != '\n')
                    ; // clear stdin
                continue;
            }
            while (getchar() != '\n')
                ; // consume leftover newline

            if (role_choice == 3)
                continue; // exit to main menu
            if (role_choice < 1 || role_choice > 3)
            {
                printf("Invalid choice.\n");
                continue;
            }

            char username[64], password[64], mobile[32];
            printf("Enter Username: ");
            if (!fgets(username, sizeof(username), stdin))
                break;
            username[strcspn(username, "\n")] = 0;
            trim(username);

            printf("Enter Password: ");
            if (!fgets(password, sizeof(password), stdin))
                break;
            password[strcspn(password, "\n")] = 0;
            trim(password);

            printf("Enter Mobile Number: ");
            if (!fgets(mobile, sizeof(mobile), stdin))
                break;
            mobile[strcspn(mobile, "\n")] = 0;
            trim(mobile);

            // roles array should have indices for employee=1 and manager=2 as per your code
            sprintf(buffer, "signup|%s|%s|%s|%s", roles[role_choice], username, password, mobile);
            send(sock, buffer, strlen(buffer), 0);

            // Read full response including assigned ID from server
            int valread = read(sock, buffer, sizeof(buffer) - 1);
            if (valread <= 0)
            {
                printf("Disconnected from server.\n");
                break;
            }
            buffer[valread] = '\0';

            printf("Server: %s\n", buffer);
        }
        else
        {
            printf("Invalid menu choice.\n");
        }
    }
    close(sock);
    return 0;
}
