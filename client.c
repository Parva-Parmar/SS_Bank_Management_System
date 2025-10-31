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
    printf("2. Exit\n");
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

        if (main_choice == 2)
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
                    {
                        // Prompt for sender's (your) account number
                        valread = read(sock, buffer, 1024);
                        buffer[valread] = '\0';
                        printf("%s", buffer);

                        char sender_account[64];
                        fgets(sender_account, sizeof(sender_account), stdin);
                        sender_account[strcspn(sender_account, "\n")] = 0;
                        send(sock, sender_account, strlen(sender_account), 0);

                        // Prompt for recipient's account number
                        valread = read(sock, buffer, 1024);
                        buffer[valread] = '\0';
                        printf("%s", buffer);

                        char recipient_account[64];
                        fgets(recipient_account, sizeof(recipient_account), stdin);
                        recipient_account[strcspn(recipient_account, "\n")] = 0;
                        send(sock, recipient_account, strlen(recipient_account), 0);

                        // Prompt for transfer amount
                        valread = read(sock, buffer, 1024);
                        buffer[valread] = '\0';
                        printf("%s", buffer);

                        float transfer_amount;
                        scanf("%f", &transfer_amount);
                        getchar(); // consume newline
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
                    else if (cust_choice == 9)
                    {
                        char buffer[1024];
                        int valread;

                        // Read logout success message from server
                        valread = read(sock, buffer, sizeof(buffer) - 1);
                        if (valread <= 0)
                        {
                            printf("Disconnected from server.\n");
                            break;
                        }
                        buffer[valread] = '\0';
                        printf("%s", buffer); // prints "Logged out successfully.\n"

                        // Connection will be closed by the server, so client should close socket
                        close(sock);

                        // Exit client or prompt for login again based on your app flow
                        exit(0); // or return to login prompt loop
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

                    if (cust_choice == 9 || cust_choice == 10)
                        break;
                }
            }
            else if (strstr(buffer, "Login successful.") && role_choice == 3)
            {
                while (1)
                {
                    valread = read(sock, buffer, 1024);
                    if (valread <= 0)
                        break;
                    buffer[valread] = '\0';
                    printf("%s", buffer);

                    int emp_choice = -1;
                    char input[32];

                    // Robust input loop for menu choice
                    while (emp_choice < 1 || emp_choice > 9)
                    {
                        printf("Enter choice: ");
                        if (!fgets(input, sizeof(input), stdin))
                        {
                            printf("Input error. Exiting menu.\n");
                            break;
                        }
                        if (input[0] == '\n')
                            continue; // ignore empty input
                        input[strcspn(input, "\n")] = 0;
                        emp_choice = atoi(input);

                        if (emp_choice < 1 || emp_choice > 9)
                            printf("Invalid choice, please enter a number between 1 and 9.\n");
                    }
                    if (emp_choice < 1 || emp_choice > 9)
                        break; // exit outer while if input loop broke

                    char cmd[32];
                    sprintf(cmd, "%d", emp_choice);
                    send(sock, cmd, strlen(cmd), 0);

                    switch (emp_choice)
                    {
                    case 1:
                    {
                        char buffer[1024];
                        char input[256];
                        int valread;

                        // Receive and print prompt for username
                        valread = recv(sock, buffer, sizeof(buffer) - 1, 0);
                        if (valread <= 0)
                            break;
                        buffer[valread] = '\0';
                        printf("%s", buffer);
                        fflush(stdout);

                        // Read username from user and send to server
                        if (!fgets(input, sizeof(input), stdin))
                            break;
                        input[strcspn(input, "\n")] = 0; // Remove newline
                        send(sock, input, strlen(input), 0);

                        // Receive and print prompt for password
                        valread = recv(sock, buffer, sizeof(buffer) - 1, 0);
                        if (valread <= 0)
                            break;
                        buffer[valread] = '\0';
                        printf("%s", buffer);
                        fflush(stdout);

                        // Read password from user and send to server
                        if (!fgets(input, sizeof(input), stdin))
                            break;
                        input[strcspn(input, "\n")] = 0;
                        send(sock, input, strlen(input), 0);

                        // Receive and print prompt for mobile number
                        valread = recv(sock, buffer, sizeof(buffer) - 1, 0);
                        if (valread <= 0)
                            break;
                        buffer[valread] = '\0';
                        printf("%s", buffer);
                        fflush(stdout);

                        // Read mobile number from user and send to server
                        if (!fgets(input, sizeof(input), stdin))
                            break;
                        input[strcspn(input, "\n")] = 0;
                        send(sock, input, strlen(input), 0);

                        // Receive and print operation result message
                        valread = recv(sock, buffer, sizeof(buffer) - 1, 0);
                        if (valread <= 0)
                            break;
                        buffer[valread] = '\0';
                        printf("%s", buffer);
                        fflush(stdout);

                        // Receive and print employee menu prompt again
                        valread = recv(sock, buffer, sizeof(buffer) - 1, 0);
                        if (valread <= 0)
                            break;
                        buffer[valread] = '\0';
                        printf("%s", buffer);
                        fflush(stdout);

                        break;
                    }

                    case 2:
                    {
                        char buffer[1024];
                        char input[256];
                        int valread;

                        // Prompt for username
                        valread = recv(sock, buffer, sizeof(buffer) - 1, 0);
                        if (valread <= 0)
                            break;
                        buffer[valread] = '\0';
                        printf("%s", buffer);
                        fflush(stdout);

                        if (!fgets(input, sizeof(input), stdin))
                            break;
                        input[strcspn(input, "\n")] = 0;
                        send(sock, input, strlen(input), 0);

                        // Prompt for password / NO
                        valread = recv(sock, buffer, sizeof(buffer) - 1, 0);
                        if (valread <= 0)
                            break;
                        buffer[valread] = '\0';
                        printf("%s", buffer);
                        fflush(stdout);

                        if (!fgets(input, sizeof(input), stdin))
                            break;
                        input[strcspn(input, "\n")] = 0;
                        send(sock, input, strlen(input), 0);

                        // Prompt for mobile / NO
                        valread = recv(sock, buffer, sizeof(buffer) - 1, 0);
                        if (valread <= 0)
                            break;
                        buffer[valread] = '\0';
                        printf("%s", buffer);
                        fflush(stdout);

                        if (!fgets(input, sizeof(input), stdin))
                            break;
                        input[strcspn(input, "\n")] = 0;
                        send(sock, input, strlen(input), 0);

                        // Receive modification result
                        valread = recv(sock, buffer, sizeof(buffer) - 1, 0);
                        if (valread <= 0)
                            break;
                        buffer[valread] = '\0';
                        printf("%s", buffer);
                        fflush(stdout);

                        // Receive menu prompt again for next action
                        valread = recv(sock, buffer, sizeof(buffer) - 1, 0);
                        if (valread <= 0)
                            break;
                        buffer[valread] = '\0';
                        printf("%s", buffer);
                        fflush(stdout);

                        break;
                    }

                    case 3:
                    {
                        char buffer[4096];
                        int valread;

                        printf("Fetching loans assigned to you...\n");

                        // Receive the full list of assigned loans
                        valread = recv(sock, buffer, sizeof(buffer) - 1, 0);
                        if (valread <= 0)
                            break;
                        buffer[valread] = '\0';
                        printf("%s", buffer);
                        fflush(stdout);

                        printf("These were the loans assigned to you.\n");

                        // Receive the employee menu prompt again
                        valread = recv(sock, buffer, sizeof(buffer) - 1, 0);
                        if (valread <= 0)
                            break;
                        buffer[valread] = '\0';
                        printf("%s", buffer);
                        fflush(stdout);

                        break;
                    }

                    case 4:
                    {
                        char buffer[1024];
                        char input[256];
                        int valread;

                        // Prompt user to enter loan ID to reject
                        printf("Enter loan ID to reject: ");
                        if (!fgets(input, sizeof(input), stdin))
                            break;
                        input[strcspn(input, "\n")] = 0; // Remove newline character

                        // Send the loan ID to server
                        send(sock, input, strlen(input), 0);

                        // Receive server response (success or error message)
                        valread = recv(sock, buffer, sizeof(buffer) - 1, 0);
                        if (valread <= 0)
                            break;
                        buffer[valread] = '\0';
                        printf("%s", buffer);
                        fflush(stdout);

                        // Receive and display the employee menu again
                        valread = recv(sock, buffer, sizeof(buffer) - 1, 0);
                        if (valread <= 0)
                            break;
                        buffer[valread] = '\0';
                        printf("%s", buffer);
                        fflush(stdout);
                        break;
                    }

                    case 5:
                    {
                        char buffer[1024];
                        char input[256];
                        int valread;

                        // Print prompt locally only on client
                        printf("Enter loan ID to approve: ");

                        // Read input from user
                        if (!fgets(input, sizeof(input), stdin))
                            break;
                        input[strcspn(input, "\n")] = 0; // Remove newline

                        // Send loan ID to server
                        send(sock, input, strlen(input), 0);

                        // Receive and print server response message (success/error)
                        valread = recv(sock, buffer, sizeof(buffer) - 1, 0);
                        if (valread <= 0)
                            break;
                        buffer[valread] = '\0';
                        printf("%s", buffer);
                        fflush(stdout);

                        // Receive and print employee menu prompt
                        valread = recv(sock, buffer, sizeof(buffer) - 1, 0);
                        if (valread <= 0)
                            break;
                        buffer[valread] = '\0';
                        printf("%s", buffer);
                        fflush(stdout);

                        break;
                    }
                    case 6:
                        // Implement view_assigned_loan_applications if needed
                        break;

                    case 7: // Change Password
                    {
                        char buffer[1024];
                        char input[256];
                        int valread;

                        // Print prompt locally and read new password from user
                        printf("Enter new password: ");
                        if (!fgets(input, sizeof(input), stdin))
                            break;
                        input[strcspn(input, "\n")] = 0; // Remove newline

                        // Send new password to server
                        send(sock, input, strlen(input), 0);

                        // Receive server response (success/failure message)
                        valread = recv(sock, buffer, sizeof(buffer) - 1, 0);
                        if (valread <= 0)
                            break;
                        buffer[valread] = '\0';
                        printf("%s", buffer);
                        fflush(stdout);

                        // Receive employee menu prompt again
                        valread = recv(sock, buffer, sizeof(buffer) - 1, 0);
                        if (valread <= 0)
                            break;
                        buffer[valread] = '\0';
                        printf("%s", buffer);
                        fflush(stdout);

                        break;
                    }
                    case 8:
                    {
                        char buffer[1024];
                        int valread;

                        // Read logout success message from server
                        valread = read(sock, buffer, sizeof(buffer) - 1);
                        if (valread <= 0)
                        {
                            printf("Disconnected from server.\n");
                            break;
                        }
                        buffer[valread] = '\0';
                        printf("%s", buffer); // prints "Logged out successfully.\n"

                        // Connection will be closed by the server, so client should close socket
                        close(sock);

                        // Exit client or prompt for login again based on your app flow
                        exit(0); // or return to login prompt loop
                        break;
                    }
                    default:
                    {
                        valread = recv(sock, buffer, sizeof(buffer) - 1, 0);
                        if (valread <= 0)
                            break;
                        buffer[valread] = '\0';
                        printf("%s", buffer);
                        fflush(stdout);
                        break;
                    }
                    }
                }
            }

            else if (strstr(buffer, "Login successful.") && role_choice == 2)
            {
                while (1)
                {
                    valread = read(sock, buffer, 1024);
                    if (valread <= 0)
                        break;
                    buffer[valread] = '\0';
                    printf("%s", buffer);

                    int manager_choice;
                    fflush(stdout);
                    scanf("%d", &manager_choice);
                    getchar(); // consume newline

                    char cmd[32];
                    snprintf(cmd, sizeof(cmd), "%d", manager_choice);
                    send(sock, cmd, strlen(cmd), 0);

                    switch (manager_choice)
                    {
                    case 1:
                        // Activate/Deactivate Customer Accounts
                        {
                            char buffer[1024];
                            char input[128];
                            int valread;

                            // printf("[CLIENT DEBUG] Starting Activate/Deactivate Customer Account flow\n");

                            // Receive prompt for account number
                            valread = recv(sock, buffer, sizeof(buffer) - 1, 0);
                            if (valread <= 0)
                            {
                                // printf("[CLIENT DEBUG] Failed to receive account number prompt\n");
                                break;
                            }
                            buffer[valread] = '\0';
                            // printf("[CLIENT DEBUG] Received prompt: %s\n", buffer);

                            // Show prompt to user
                            printf("%s", buffer);

                            // Read user input
                            if (!fgets(input, sizeof(input), stdin))
                            {
                                // printf("[CLIENT DEBUG] Failed to read user input for account number\n");
                                break;
                            }
                            input[strcspn(input, "\n")] = 0;
                            send(sock, input, strlen(input), 0);
                            // printf("[CLIENT DEBUG] Sent account number: %s\n", input);

                            // Receive prompt for status (Active/Deactive)
                            valread = recv(sock, buffer, sizeof(buffer) - 1, 0);
                            if (valread <= 0)
                            {
                                // printf("[CLIENT DEBUG] Failed to receive status prompt\n");
                                break;
                            }
                            buffer[valread] = '\0';
                            // printf("[CLIENT DEBUG] Received prompt: %s\n", buffer);

                            // Show prompt to user
                            printf("%s", buffer);

                            // Read status input
                            if (!fgets(input, sizeof(input), stdin))
                            {
                                // printf("[CLIENT DEBUG] Failed to read user input for status\n");
                                break;
                            }
                            input[strcspn(input, "\n")] = 0;
                            send(sock, input, strlen(input), 0);
                            // printf("[CLIENT DEBUG] Sent status choice: %s\n", input);

                            // Receive final response from server
                            valread = recv(sock, buffer, sizeof(buffer) - 1, 0);
                            if (valread <= 0)
                            {
                                // printf("[CLIENT DEBUG] Failed to receive final response\n");
                                break;
                            }
                            buffer[valread] = '\0';
                            // printf("[CLIENT DEBUG] Received final response: %s\n", buffer);

                            // Display final response to user
                            printf("%s", buffer);
                        }
                        break;

                    case 2:
                    {
                        char buffer[1024];
                        char input[128];
                        int valread;

                        // Receive prompt for Loan ID from server
                        valread = recv(sock, buffer, sizeof(buffer) - 1, 0);
                        if (valread <= 0)
                            break;
                        buffer[valread] = '\0';
                        printf("%s", buffer);
                        if (!fgets(input, sizeof(input), stdin))
                            break;
                        input[strcspn(input, "\n")] = 0;
                        send(sock, input, strlen(input), 0);

                        // Receive prompt for Employee ID from server
                        valread = recv(sock, buffer, sizeof(buffer) - 1, 0);
                        if (valread <= 0)
                            break;
                        buffer[valread] = '\0';
                        printf("%s", buffer);
                        if (!fgets(input, sizeof(input), stdin))
                            break;
                        input[strcspn(input, "\n")] = 0;
                        send(sock, input, strlen(input), 0);

                        // Receive result code from server
                        valread = recv(sock, buffer, sizeof(buffer) - 1, 0);
                        if (valread <= 0)
                            break;
                        buffer[valread] = '\0';

                        // Interpret result code
                        if (buffer[0] == '0')
                        {
                            printf("Loan assigned successfully.\n");
                            fflush(stdout);
                        }
                        else if (buffer[0] == '1')
                        {
                            printf("Invalid Loan ID.\n");
                            fflush(stdout);
                        }
                        else if (buffer[0] == '2')
                        {
                            printf("Invalid Employee ID.\n");
                            fflush(stdout);
                        }
                        else if (buffer[0] == '3')
                        {
                            printf("Failed to assign loan due to server error.\n");
                            fflush(stdout);
                        }
                        else
                        {
                            printf("Unknown server response: %s\n", buffer);
                        }
                    }
                    break;
                    case 3: // Review Customer Feedback
                    {
                        char buffer[1024];
                        int valread;
                        const char *end_marker = "__FEEDBACK_END__";
                        char feedback_accumulator[65536] = "";
                        printf("Start of Feedback.\n");
                        while (1)
                        {
                            valread = recv(sock, buffer, sizeof(buffer) - 1, 0);
                            if (valread <= 0)
                                break;
                            buffer[valread] = '\0';
                            strncat(feedback_accumulator, buffer, sizeof(feedback_accumulator) - strlen(feedback_accumulator) - 1);
                            if (strstr(feedback_accumulator, end_marker) != NULL)
                            {
                                break;
                            }
                        }
                        // Remove end_marker from output before displaying
                        char *pos = strstr(feedback_accumulator, end_marker);
                        if (pos)
                            *pos = '\0';

                        printf("%s\n", feedback_accumulator);
                        printf("End of Feedback.\n");
                        fflush(stdout);
                    }
                    break;
                    case 4:
                    {
                        char buffer[1024];
                        char input_buffer[128];
                        int valread;

                        // Receive prompt for username
                        valread = recv(sock, buffer, sizeof(buffer) - 1, 0);
                        if (valread <= 0)
                            break;
                        buffer[valread] = '\0';
                        printf("%s", buffer);
                        fflush(stdout);

                        // Get and send username
                        if (fgets(input_buffer, sizeof(input_buffer), stdin) == NULL)
                            break;
                        input_buffer[strcspn(input_buffer, "\n")] = 0;
                        send(sock, input_buffer, strlen(input_buffer), 0);

                        // Receive prompt for new password
                        valread = recv(sock, buffer, sizeof(buffer) - 1, 0);
                        if (valread <= 0)
                            break;
                        buffer[valread] = '\0';
                        printf("%s", buffer);
                        fflush(stdout);

                        // Get and send new password
                        if (fgets(input_buffer, sizeof(input_buffer), stdin) == NULL)
                            break;
                        input_buffer[strcspn(input_buffer, "\n")] = 0;
                        send(sock, input_buffer, strlen(input_buffer), 0);

                        // Receive and print server response
                        valread = recv(sock, buffer, sizeof(buffer) - 1, 0);
                        if (valread <= 0)
                            break;
                        buffer[valread] = '\0';
                        printf("%s", buffer);
                        fflush(stdout);
                    }
                    break;

                    case 5:
                        // Logout
                        printf("Logging out...\n");
                        break;

                    default:
                        valread = read(sock, buffer, 1024);
                        if (valread <= 0)
                            break;
                        buffer[valread] = '\0';
                        printf("%s", buffer);
                        break;
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
        else
        {
            printf("Invalid menu choice.\n");
        }
    }
    close(sock);
    return 0;
}
