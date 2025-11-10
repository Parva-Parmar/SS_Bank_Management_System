#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include <fcntl.h>
#include "filelock.h"
#include "customer.h"
#include "employee.h"
#include "manager.h"
#include "admin.h"

#define MAX_USERS 100
char active_users[MAX_USERS][64];
int active_user_count = 0;
pthread_mutex_t active_users_mutex = PTHREAD_MUTEX_INITIALIZER;

#define PORT 8080
#define SERVER_LOG_FILE "server.log"
#define USERS_FILE "users.txt"
#define CUSTOMER_DATA_FILE "customer_data.txt"
#define EMPLOYEE_FILE "employee_data.txt"
#define MANAGER_FILE "manager_data.txt"

pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
char logged_in_username[64] = {0}; // store username after login

int is_user_logged_in(const char *username)
{
    int i, found = 0;
    pthread_mutex_lock(&active_users_mutex);
    for (i = 0; i < active_user_count; i++)
    {
        if (strcmp(active_users[i], username) == 0)
        {
            found = 1;
            break;
        }
    }
    pthread_mutex_unlock(&active_users_mutex);
    return found;
}

int add_active_user(const char *username)
{
    pthread_mutex_lock(&active_users_mutex);
    if (active_user_count >= MAX_USERS)
    { // no space
        pthread_mutex_unlock(&active_users_mutex);
        return -1;
    }
    strcpy(active_users[active_user_count++], username);
    pthread_mutex_unlock(&active_users_mutex);
    return 0;
}

// Call this on user logout or disconnect

void remove_active_user(const char *username)
{
    int i;
    pthread_mutex_lock(&active_users_mutex);
    for (i = 0; i < active_user_count; i++)
    {
        if (strcmp(active_users[i], username) == 0)
        {
            // Replace with last user to keep array compact
            strcpy(active_users[i], active_users[active_user_count - 1]);
            active_user_count--;
            break;
        }
    }
    pthread_mutex_unlock(&active_users_mutex);
}
void handle_user_logout(const char *username)
{
    remove_active_user(username);
    // Optional: additional cleanup if needed
}

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

void write_server_log(const char *client_ip, const char *request, const char *response)
{
    pthread_mutex_lock(&log_mutex);
    FILE *fp = fopen(SERVER_LOG_FILE, "a");
    if (!fp)
    {
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

void generate_next_id(const char *filename, char prefix, char *out_id, size_t out_id_size)
{
    FILE *fp = fopen(filename, "r");
    if (!fp)
    {
        // File doesn't exist, start from 1
        snprintf(out_id, out_id_size, "%c000001", prefix);
        return;
    }
    char line[256];
    int max_num = 0;
    while (fgets(line, sizeof(line), fp))
    {
        char id[16];
        if (sscanf(line, "%15s", id) == 1 && id[0] == prefix)
        {
            int num = atoi(id + 1);
            if (num > max_num)
                max_num = num;
        }
    }
    fclose(fp);

    snprintf(out_id, out_id_size, "%c%06d", prefix, max_num + 1);
}

void handle_client(int new_socket)
{
    char buffer[1024] = {0};
    char response[1024] = {0};
    char client_ip[INET_ADDRSTRLEN] = "Unknown";

    while (1)
    {
        int valread = read(new_socket, buffer, 1024);
        if (valread <= 0)
            break;
        buffer[valread] = '\0';
        printf("DEBUG: Received raw request string: [%s]\n", buffer);
        if (strncmp(buffer, "exit", 4) == 0)
            break;

        char action[16], role[32], username[64], password[64], mobile[32];
        sscanf(buffer, "%[^|]|%[^|]|%[^|]|%[^|]|%[^|]", action, role, username, password, mobile);

        trim(action);
        trim(role);
        trim(username);
        trim(password);
        trim(mobile);
        printf("DEBUG: Parsed values -> action: [%s], role: [%s], username: [%s], password: [%s], mobile: [%s]\n", action, role, username, password, mobile);
        FILE *fp = NULL;
        printf("DEBUG: Checking role = %s\n", role);
        if (is_user_logged_in(username))
        {
            char *msg = "User already logged in from another terminal.\n";
            send(new_socket, msg, strlen(msg), 0);
            close(new_socket);
            pthread_exit(NULL);
        }
        if (strcmp(action, "login") == 0)
        {
            if (strcmp(role, "customer") == 0)
            {
                printf("DEBUG: Handling customer login\n");
                if (validate_customer_login(username, password))
                {
                    if (add_active_user(username) != 0)
                    {
                        char *msg = "Maximum active users reached. Try again later.\n";
                        send(new_socket, msg, strlen(msg), 0);
                        close(new_socket);
                        pthread_exit(NULL);
                    }
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
                        "Enter choice: ";

                    while (1)
                    {
                        send(new_socket, cust_menu, strlen(cust_menu), 0);
                        valread = read(new_socket, buffer, sizeof(buffer) - 1);
                        if (valread <= 0)
                            break;
                        buffer[valread] = '\0';
                        int choice = atoi(buffer);

                        switch (choice)
                        {
                        case 1:
                        {
                            float balance = get_account_balance(username);
                            if (balance < 0)
                                snprintf(response, sizeof(response), "Error retrieving balance.\n");
                            else
                                snprintf(response, sizeof(response), "Your account balance is: %.2f\n", balance);
                            send(new_socket, response, strlen(response), 0);
                            write_server_log(client_ip, "View Account Balance", response);
                            break;
                        }
                        case 2:
                        {
                            strcpy(response, "Proceding to deposit money.\n");
                            send(new_socket, response, strlen(response), 0);
                            valread = read(new_socket, buffer, sizeof(buffer) - 1);
                            if (valread <= 0)
                                break;
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
                        case 3:
                        { // Assuming 3 is withdrawal
                            // send(new_socket, "Enter withdrawal amount: ", 25, 0);
                            strcpy(response, "Proceding to withdraw money.\n");
                            send(new_socket, response, strlen(response), 0);
                            int amt_len = read(new_socket, buffer, sizeof(buffer) - 1);
                            buffer[amt_len] = '\0';

                            float amount = atof(buffer);
                            bool res = withdraw_money(username, amount);
                            if (res)
                                strcpy(response, "Withdrawal successful.\n");
                            else
                                strcpy(response, "Withdrawal failed: insufficient funds.\n");

                            send(new_socket, response, strlen(response), 0);
                            write_server_log(client_ip, "Withdraw Money", response);
                            break;
                        }
                        case 4:
                        { // Transfer Funds
                            // Get sender's account number from username (logged in user)
                            char sender_account[64];
                            bool found_account = false;

                            FILE *fp = fopen("customer_data.txt", "r");
                            if (!fp)
                            {
                                char *msg = "Failed to open customer data file.\n";
                                send(new_socket, msg, strlen(msg), 0);
                                break;
                            }

                            int fd = fileno(fp);
                            if (lock_file(fd, F_RDLCK) != 0)
                            {
                                fclose(fp);
                                char *msg = "Failed to lock customer data file.\n";
                                send(new_socket, msg, strlen(msg), 0);
                                break;
                            }

                            char line[256];
                            while (fgets(line, sizeof(line), fp))
                            {
                                char file_user[64], file_acc[32], mobile[32], status[32];
                                float balance;
                                int parsed = sscanf(line, "%63[^|]|%31[^|]|%31[^|]|%f|%31[^\n]", file_user, file_acc, mobile, &balance, status);
                                if (parsed == 5)
                                {
                                    trim(file_user);
                                    if (strcmp(file_user, username) == 0)
                                    {
                                        strncpy(sender_account, file_acc, sizeof(sender_account) - 1);
                                        sender_account[sizeof(sender_account) - 1] = '\0';
                                        found_account = true;
                                        break;
                                    }
                                }
                            }

                            unlock_file(fd);
                            fclose(fp);

                            if (!found_account)
                            {
                                char *msg = "Sender account not found.\n";
                                send(new_socket, msg, strlen(msg), 0);
                                break;
                            }

                            // Ask for recipient account number
                            char prompt[64] = "Enter recipient account number: ";
                            send(new_socket, prompt, strlen(prompt), 0);

                            int len = read(new_socket, buffer, sizeof(buffer) - 1);
                            if (len <= 0)
                                break;
                            buffer[len] = '\0';
                            trim(buffer);
                            char recipient_account[64];
                            strcpy(recipient_account, buffer);

                            // Ask for transfer amount
                            strcpy(prompt, "Enter transfer amount: ");
                            send(new_socket, prompt, strlen(prompt), 0);

                            len = read(new_socket, buffer, sizeof(buffer) - 1);
                            if (len <= 0)
                                break;
                            buffer[len] = '\0';
                            float amount = atof(buffer);

                            bool success = transfer_funds_with_account(sender_account, recipient_account, amount);

                            if (success)
                                strcpy(response, "Transfer successful.\n");
                            else
                                strcpy(response, "Transfer failed. Check account numbers or insufficient funds.\n");

                            send(new_socket, response, strlen(response), 0);
                            write_server_log(client_ip, "Transfer Funds", response);

                            break;
                        }

                        case 5:
                        { // Apply for a Loan
                            // Send prompt for loan amount
                            char prompt[] = "Enter loan amount to apply for: ";
                            send(new_socket, prompt, strlen(prompt), 0);

                            // Read loan amount from client
                            int len = read(new_socket, buffer, sizeof(buffer) - 1);
                            if (len <= 0)
                                break;
                            buffer[len] = '\0';

                            float loan_amount = atof(buffer);

                            bool success = apply_for_loan(username, loan_amount);

                            if (success)
                                strcpy(response, "Loan application submitted.\n");
                            else
                                strcpy(response, "Loan application failed or existing loan present.\n");

                            send(new_socket, response, strlen(response), 0);
                            write_server_log(client_ip, "Apply for Loan", response);
                            break;
                        }
                        case 6:
                        { // Change Password
                            char prompt[] = "Enter new password: ";
                            send(new_socket, prompt, strlen(prompt), 0);

                            int len = read(new_socket, buffer, sizeof(buffer) - 1);
                            if (len <= 0)
                                break;

                            buffer[len] = '\0';

                            bool success = customer_change_password(username, buffer);

                            if (success)
                                strcpy(response, "Password changed successfully.\n");
                            else
                                strcpy(response, "Password change failed.\n");

                            send(new_socket, response, strlen(response), 0);
                            write_server_log(client_ip, "Change Password", response);

                            break;
                        }
                        case 7:
                        { // Add Feedback
                            char prompt[] = "Enter your feedback: ";
                            send(new_socket, prompt, strlen(prompt), 0);

                            int len = read(new_socket, buffer, sizeof(buffer) - 1);
                            if (len <= 0)
                                break;
                            buffer[len] = '\0';

                            bool success = add_feedback(username, buffer);
                            if (success)
                                strcpy(response, "Thank you for your feedback.\n");
                            else
                                strcpy(response, "Failed to submit feedback.\n");

                            send(new_socket, response, strlen(response), 0);
                            write_server_log(client_ip, "Add Feedback", response);
                            break;
                        }
                        case 8:
                        { // View Transaction History
                            char history[4096];
                            bool success = get_transaction_history(username, history, sizeof(history));
                            if (success)
                            {
                                send(new_socket, history, strlen(history), 0);
                            }
                            else
                            {
                                char *no_history = "No transaction history found.\n";
                                send(new_socket, no_history, strlen(no_history), 0);
                            }
                            write_server_log(client_ip, "View Transaction History", success ? "History sent" : "No history");
                            break;
                        }
                        case 9:
                        {
                            // Assume username stored per client session
                            handle_user_logout(username);

                            // Send logout success message
                            char *msg = "Logged out successfully.\n";
                            send(new_socket, msg, strlen(msg), 0);

                            // Close socket and terminate thread/session handling below
                            close(new_socket);
                            pthread_exit(NULL);
                        }
                        default:
                            strcpy(response, "Functionality not yet implemented.\n");
                            send(new_socket, response, strlen(response), 0);
                            write_server_log(client_ip, "Other Customer Menu Choice", response);
                            break;
                        }
                    }
                }
                else
                {
                    printf("DEBUG: customer login failed.\n");
                    strcpy(response, "Login failed.\n");
                    send(new_socket, response, strlen(response), 0);
                    write_server_log(client_ip, buffer, response);
                }
            }
            else if (strcmp(role, "employee") == 0)
            {
                printf("DEBUG: Entered employee login branch for user %s\n", username);
                if (validate_employee_login(username, password))
                {
                    if (add_active_user(username) != 0)
                    {
                        const char *msg = "Maximum active users reached. Try again later.\n";
                        send(new_socket, msg, strlen(msg), 0);
                        close(new_socket);
                        pthread_exit(NULL);
                    }

                    const char *login_success = "Login successful.\n";
                    send(new_socket, login_success, strlen(login_success), 0);
                    write_server_log(client_ip, buffer, login_success);

                    const char emp_menu[] =
                        "Employee Menu:\n"
                        "1. Add New Customer\n"
                        "2. Modify Customer Details\n"
                        "3. View Loan Applications\n"
                        "4. Reject Loan\n"
                        "5. Approve Loan\n"
                        "6. View Customer Transactions\n"
                        "7. Change Password\n"
                        "8. Logout\n"
                        "\nEnter choice: \n";

                    while (1)
                    {
                        // Send menu prompt once per iteration
                        send(new_socket, emp_menu, strlen(emp_menu), 0);

                        int valread = read(new_socket, buffer, sizeof(buffer) - 1);
                        if (valread <= 0)
                            break;
                        buffer[valread] = '\0';
                        int choice = atoi(buffer);

                        switch (choice)
                        {
                        case 1:
                        {
                            // Add New Customer handling: prompt, read, send messages only
                            char add_new_username[64], add_new_password[64], add_new_mobile[16], account_number[32];
                            FILE *fp;
                            long max_account_num = 1000000;

                            const char *prompt_username = "Enter new customer username:\n";
                            send(new_socket, prompt_username, strlen(prompt_username), 0);
                            valread = read(new_socket, add_new_username, sizeof(add_new_username) - 1);
                            if (valread <= 0)
                                break;
                            add_new_username[valread] = '\0';
                            trim(add_new_username);

                            const char *prompt_password = "Enter password:\n";
                            send(new_socket, prompt_password, strlen(prompt_password), 0);
                            valread = read(new_socket, add_new_password, sizeof(add_new_password) - 1);
                            if (valread <= 0)
                                break;
                            add_new_password[valread] = '\0';
                            trim(add_new_password);

                            const char *prompt_mobile = "Enter mobile number:\n";
                            send(new_socket, prompt_mobile, strlen(prompt_mobile), 0);
                            valread = read(new_socket, add_new_mobile, sizeof(add_new_mobile) - 1);
                            if (valread <= 0)
                                break;
                            add_new_mobile[valread] = '\0';
                            trim(add_new_mobile);

                            fp = fopen(CUSTOMER_DATA_FILE, "r");
                            if (fp)
                            {
                                char line[256];
                                while (fgets(line, sizeof(line), fp))
                                {
                                    char username[64], mobile[16], status[32];
                                    float balance;
                                    long acct_num;
                                    if (sscanf(line, "%63[^|]|%ld|%15[^|]|%f|%31s",
                                               username, &acct_num, mobile, &balance, status) == 5)
                                    {
                                        if (acct_num >= max_account_num)
                                            max_account_num = acct_num + 1;
                                    }
                                }
                                fclose(fp);
                            }
                            snprintf(account_number, sizeof(account_number), "%ld", max_account_num);

                            int result = add_new_customer(add_new_username, add_new_password, account_number, add_new_mobile);

                            if (result == 0)
                                send(new_socket, "New customer added successfully.\n", 32, 0);
                            else if (result == -2)
                                send(new_socket, "Username already exists. Try another.\n", 38, 0);
                            else
                                send(new_socket, "Failed to add new customer.\n", 27, 0);

                            break;
                        }
                        case 2:
                        {
                            // Modify Customer Details handling similar structure, no extra prompt send here
                            char username[64], new_password[64], new_mobile[32];
                            char response[256];

                            const char *prompt_username = "Enter username: ";
                            send(new_socket, prompt_username, strlen(prompt_username), 0);
                            valread = read(new_socket, username, sizeof(username) - 1);
                            if (valread <= 0)
                                break;
                            username[valread] = '\0';
                            trim(username);

                            const char *prompt_password = "Enter new password or write 'NO' to keep unchanged: ";
                            send(new_socket, prompt_password, strlen(prompt_password), 0);
                            valread = read(new_socket, new_password, sizeof(new_password) - 1);
                            if (valread <= 0)
                                break;
                            new_password[valread] = '\0';
                            trim(new_password);
                            if (strcasecmp(new_password, "NO") == 0)
                                new_password[0] = '\0';

                            const char *prompt_mobile = "Enter new mobile number or write 'NO' to keep unchanged: ";
                            send(new_socket, prompt_mobile, strlen(prompt_mobile), 0);
                            valread = read(new_socket, new_mobile, sizeof(new_mobile) - 1);
                            if (valread <= 0)
                                break;
                            new_mobile[valread] = '\0';
                            trim(new_mobile);
                            if (strcasecmp(new_mobile, "NO") == 0)
                                new_mobile[0] = '\0';

                            int mod_result = modify_user_details("customer", username, new_password, new_mobile);

                            if (mod_result == 0)
                                strcpy(response, "User details updated successfully.\n");
                            else
                                strcpy(response, "Failed to update user details.\n");

                            send(new_socket, response, strlen(response), 0);
                            write_server_log(client_ip, "Modify User Details", response);

                            break;
                        }
                        case 3:
                        {
                            char loans_info[4096];
                            bool success = get_assigned_loans(username, loans_info, sizeof(loans_info));
                            if (success)
                                send(new_socket, loans_info, strlen(loans_info), 0);
                            else
                            {
                                const char *no_loans = "No loan applications assigned to you currently.\n";
                                send(new_socket, no_loans, strlen(no_loans), 0);
                            }
                            // send(new_socket, emp_menu, strlen(emp_menu), 0);
                            break;
                        }
                        case 4:
                        {
                            // Prompt for loan ID to reject
                            const char *prompt = "Enter loan ID to reject: ";
                            send(new_socket, prompt, strlen(prompt), 0);

                            // Read loan ID from client
                            int valread = read(new_socket, buffer, sizeof(buffer) - 1);
                            if (valread <= 0)
                                break;
                            buffer[valread] = '\0';
                            trim(buffer); // Trim whitespace/newlines if you have trim function

                            // Call reject function with username and loan ID
                            int result = reject_loan_by_id(buffer, username);

                            // Prepare response based on result
                            const char *response;
                            switch (result)
                            {
                            case 0:
                                response = "Loan rejected successfully.\n";
                                break;
                            case 1:
                                response = "Loan ID not found.\n";
                                break;
                            case 2:
                                response = "Loan not assigned to you.\n";
                                break;
                            case 3:
                                response = "Loan already approved, cannot reject.\n";
                                break;
                            default:
                                response = "Failed to reject loan due to unknown error.\n";
                                break;
                            }

                            // Send response to client
                            send(new_socket, response, strlen(response), 0);

                            // send(new_socket, emp_menu, strlen(emp_menu), 0);
                            write_server_log(client_ip, "Reject Loan", response);
                            break;
                        }
                        case 5:
                        {
                            // Prompt employee for the loan ID to approve
                            const char *prompt = "Enter loan ID to approve: ";
                            send(new_socket, prompt, strlen(prompt), 0);

                            // Read loan ID from client
                            int valread = read(new_socket, buffer, sizeof(buffer) - 1);
                            if (valread <= 0)
                                break; // connection closed or error
                            buffer[valread] = '\0';
                            trim(buffer); // trim whitespace/newline characters

                            // Call the approval function with loan ID and logged-in username
                            int result = approve_loan_by_id(buffer, username);

                            // Prepare response message based on function return value
                            const char *response;
                            switch (result)
                            {
                            case 0:
                                response = "Loan approved successfully.\n";
                                break;
                            case 1:
                                response = "Loan ID not found.\n";
                                break;
                            case 2:
                                response = "Loan not assigned to you.\n";
                                break;
                            case 3:
                                response = "Loan already approved.\n";
                                break;
                            default:
                                response = "Failed to approve loan due to unknown error.\n";
                                break;
                            }

                            // Send response back to client
                            send(new_socket, response, strlen(response), 0);
                            write_server_log(client_ip, "Approve Loan", response);

                            break;
                        }
                        case 6:
                        {
                            // Prompt for account number
                            const char *prompt = "Enter customer account number to view transactions: ";
                            send(new_socket, prompt, strlen(prompt), 0);

                            // Read account number from client
                            int valread = read(new_socket, buffer, sizeof(buffer) - 1);
                            if (valread <= 0)
                                break; // Connection closed or error
                            buffer[valread] = '\0';
                            trim(buffer); // Trim whitespace and newlines if you have a trim function

                            // Allocate buffer for transaction history
                            char tx_history[8192]; // Adjust size as needed
                            bool success = get_transaction_history_by_account(buffer, tx_history, sizeof(tx_history));

                            if (success)
                            {
                                send(new_socket, tx_history, strlen(tx_history), 0);
                            }
                            else
                            {
                                const char *msg = "No transaction history found for the given account.\n";
                                send(new_socket, msg, strlen(msg), 0);
                            }

                            write_server_log(client_ip, "View Customer Transactions", success ? "History sent" : "No history found");
                            break;
                        }
                        case 7:
                        { // Change Password
                            char prompt[] = "Enter new password: ";
                            send(new_socket, prompt, strlen(prompt), 0);

                            int len = read(new_socket, buffer, sizeof(buffer) - 1);
                            if (len <= 0)
                                break;

                            buffer[len] = '\0';

                            bool success = employee_change_password(username, buffer);

                            if (success)
                                strcpy(response, "Password changed successfully.\n");
                            else
                                strcpy(response, "Password change failed.\n");

                            send(new_socket, response, strlen(response), 0);
                            write_server_log(client_ip, "Change Password", response);

                            break;
                        }
                        case 8:
                        {
                            const char *msg = "Logged out successfully.\n";
                            send(new_socket, msg, strlen(msg), 0);
                            close(new_socket);
                            pthread_exit(NULL);
                        }
                        default:
                        {
                            const char *msg = "Invalid choice. Please try again.\n";
                            send(new_socket, msg, strlen(msg), 0);
                            break;
                        }
                        }
                    }
                }
                else
                {
                    const char *login_fail = "Login failed.\n";
                    send(new_socket, login_fail, strlen(login_fail), 0);
                    write_server_log(client_ip, buffer, login_fail);
                }
            }

            // else if (strcmp(role, "employee") == 0)
            // {

            //     printf("DEBUG: Entered employee login branch for user %s\n", username);
            //     int valid = validate_employee_login(username, password);
            //     printf("DEBUG: validate_employee_login returned %d\n", valid);
            //     if (valid)
            //     {
            //         // After verifying user credentials
            //         if (add_active_user(username) != 0)
            //         {
            //             char *msg = "Maximum active users reached. Try again later.\n";
            //             send(new_socket, msg, strlen(msg), 0);
            //             close(new_socket);
            //             pthread_exit(NULL);
            //         }
            //         strcpy(logged_in_username, username);
            //         printf("Debug: logged_in_username = '%s'\n", logged_in_username);
            //         printf("DEBUG: Employee login successful.\n");
            //         strcpy(response, "Login successful.\n");
            //         send(new_socket, response, strlen(response), 0);
            //         write_server_log(client_ip, buffer, response);

            //         // Employee menu string
            //         char emp_menu[] =
            //             "Employee Menu:\n"
            //             "1. Add New Customer\n"
            //             "2. Modify Customer Details\n"
            //             "3. View Loan Applications\n"
            //             "4. Reject Loan\n"
            //             "5. Approve Loan\n"
            //             "6. View Customer Transactions\n"
            //             "7. Change Password\n"
            //             "8. Logout\n"
            //             "Enter choice: ";

            //         while (1)
            //         {
            //             send(new_socket, emp_menu, strlen(emp_menu), 0);
            //             valread = read(new_socket, buffer, sizeof(buffer) - 1);
            //             if (valread <= 0)
            //                 break;
            //             buffer[valread] = '\0';
            //             int choice = atoi(buffer);

            //             char employee_id[16] = {0};
            //             FILE *fp = fopen("employee_data.txt", "r");
            //             if (!fp)
            //             {
            //                 send(new_socket, "Error opening employee data file.\n", 34, 0);
            //                 break;
            //             }
            //             char line[256];
            //             char username[64];
            //             strcpy(username, logged_in_username); // Use your logged-in username storage
            //             while (fgets(line, sizeof(line), fp))
            //             {
            //                 char id[16], uname[64], mobile[32];
            //                 if (sscanf(line, "%15[^|]|%63[^|]|%31[^\n]", id, uname, mobile) == 3)
            //                 {
            //                     if (strcmp(uname, username) == 0)
            //                     {
            //                         strcpy(employee_id, id);
            //                         break;
            //                     }
            //                 }
            //             }
            //             fclose(fp);

            //             if (employee_id[0] == '\0')
            //             {
            //                 send(new_socket, "Employee ID not found for user.\n", 32, 0);
            //                 break;
            //             }
            //             printf("Debug: emp_id = '%s'\n", employee_id);

            //             switch (choice)
            //             {
            //             case 1:
            //             {
            //                 char add_new_username[64], add_new_password[64], add_new_mobile[16], account_number[32];
            //                 FILE *fp;
            //                 long max_account_num = 1000000;

            //                 // Prompt username
            //                 const char *prompt_username = "Enter new customer username:\n";
            //                 send(new_socket, prompt_username, strlen(prompt_username), 0);
            //                 valread = read(new_socket, add_new_username, sizeof(add_new_username) - 1);
            //                 if (valread <= 0)
            //                     break;
            //                 add_new_username[valread] = '\0';
            //                 trim(add_new_username);

            //                 // Prompt password
            //                 const char *prompt_password = "Enter password:\n";
            //                 send(new_socket, prompt_password, strlen(prompt_password), 0);
            //                 valread = read(new_socket, add_new_password, sizeof(add_new_password) - 1);
            //                 if (valread <= 0)
            //                     break;
            //                 add_new_password[valread] = '\0';
            //                 trim(add_new_password);

            //                 // Prompt mobile
            //                 const char *prompt_mobile = "Enter mobile number:\n";
            //                 send(new_socket, prompt_mobile, strlen(prompt_mobile), 0);
            //                 valread = read(new_socket, add_new_mobile, sizeof(add_new_mobile) - 1);
            //                 if (valread <= 0)
            //                     break;
            //                 add_new_mobile[valread] = '\0';
            //                 trim(add_new_mobile);

            //                 // Generate unique account number
            //                 fp = fopen(CUSTOMER_DATA_FILE, "r");
            //                 if (fp)
            //                 {
            //                     char line[256];
            //                     while (fgets(line, sizeof(line), fp))
            //                     {
            //                         char username[64], mobile[16], status[32];
            //                         float balance;
            //                         long acct_num;
            //                         if (sscanf(line, "%63[^|]|%ld|%15[^|]|%f|%31s",
            //                                    username, &acct_num, mobile, &balance, status) == 5)
            //                         {
            //                             if (acct_num >= max_account_num)
            //                                 max_account_num = acct_num + 1;
            //                         }
            //                     }
            //                     fclose(fp);
            //                 }
            //                 snprintf(account_number, sizeof(account_number), "%ld", max_account_num);

            //                 // Call the new add_new_customer function
            //                 int result = add_new_customer(add_new_username, add_new_password, account_number, add_new_mobile);

            //                 if (result == 0)
            //                     send(new_socket, "New customer added successfully.\n", 32, 0);
            //                 else if (result == -2)
            //                     send(new_socket, "Username already exists. Try another.\n", 38, 0);
            //                 else
            //                     send(new_socket, "Failed to add new customer.\n", 27, 0);

            //                 // send(new_socket, emp_menu, strlen(emp_menu), 0);
            //                 break;
            //             }
            // case 2:
            // {
            //     char username[64], new_password[64], new_mobile[32];
            //     char response[256];
            //     int valread;

            //     // Prompt for user type
            //     // const char *prompt_user_type = "Modify which user type? (Type customer): \n";
            //     // send(new_socket, prompt_user_type, strlen(prompt_user_type), 0);
            //     // valread = recv(new_socket, user_type, sizeof(user_type) - 1, 0);
            //     // if (valread <= 0)
            //     //     break;
            //     // user_type[valread] = '\0';
            //     // trim(user_type);
            //     char user_type[16] = "customer";
            //     // Prompt for username
            //     const char *prompt_username = "Enter username: ";
            //     send(new_socket, prompt_username, strlen(prompt_username), 0);
            //     valread = recv(new_socket, username, sizeof(username) - 1, 0);
            //     if (valread <= 0)
            //         break;
            //     username[valread] = '\0';
            //     trim(username);

            //     // Prompt for new password or NO
            //     const char *prompt_password = "Enter new password or write 'NO' to keep unchanged: ";
            //     send(new_socket, prompt_password, strlen(prompt_password), 0);
            //     valread = recv(new_socket, new_password, sizeof(new_password) - 1, 0);
            //     if (valread <= 0)
            //         break;
            //     new_password[valread] = '\0';
            //     trim(new_password);
            //     if (strcasecmp(new_password, "NO") == 0)
            //         new_password[0] = '\0';

            //     // Prompt for new mobile or NO
            //     const char *prompt_mobile = "Enter new mobile number or write 'NO' to keep unchanged: ";
            //     send(new_socket, prompt_mobile, strlen(prompt_mobile), 0);
            //     valread = recv(new_socket, new_mobile, sizeof(new_mobile) - 1, 0);
            //     if (valread <= 0)
            //         break;
            //     new_mobile[valread] = '\0';
            //     trim(new_mobile);
            //     if (strcasecmp(new_mobile, "NO") == 0)
            //         new_mobile[0] = '\0';

            //     // Call modification function
            //     int mod_result = modify_user_details(user_type, username, new_password, new_mobile);

            //     if (mod_result == 0)
            //         strcpy(response, "User details updated successfully.\n");
            //     else
            //         strcpy(response, "Failed to update user details.\n");

            //     send(new_socket, response, strlen(response), 0);
            //     write_server_log(client_ip, "Modify User Details", response);
            //     send(new_socket, emp_menu, strlen(emp_menu), 0);
            //     break;
            // }
            //             case 3: // View Assigned Loan Applications
            //             {
            //                 // FILE *fp = fopen(EMPLOYEE_FILE, "r");
            //                 // if (!fp)
            //                 // {
            //                 //     const char *msg = "Error opening employee data file.\n";
            //                 //     send(new_socket, msg, strlen(msg), 0);
            //                 //     break;
            //                 // }

            //                 // char line[256], uname[64], id[16], mobile[16];
            //                 // while (fgets(line, sizeof(line), fp))
            //                 // {
            //                 //     if (sscanf(line, "%15s %63s %15s", id, uname, mobile) == 3)
            //                 //     {
            //                 //         if (strcmp(uname, logged_in_username) == 0)
            //                 //         {
            //                 //             strcpy(employeeid, id);
            //                 //             break;
            //                 //         }
            //                 //     }
            //                 // }
            //                 // fclose(fp);
            //                 printf("Debug: employeeid = '%s'\n", employee_id);
            //                 if (employee_id[0] == '\0')
            //                 {
            //                     const char *msg = "Employee ID not found for user.\n";
            //                     send(new_socket, msg, strlen(msg), 0);
            //                     break;
            //                 }

            //                 fp = fopen("loans.txt", "r");
            //                 if (!fp)
            //                 {
            //                     const char *msg = "Failed to open loans file.\n";
            //                     send(new_socket, msg, strlen(msg), 0);
            //                     break;
            //                 }

            //                 int fd = fileno(fp);
            //                 if (lock_file(fd, F_RDLCK) < 0)
            //                 {
            //                     fclose(fp);
            //                     const char *msg = "Failed to lock loans file.\n";
            //                     send(new_socket, msg, strlen(msg), 0);
            //                     break;
            //                 }

            //                 char loan_id[32], account_num[32], status[32], loan_emp_id[32];
            //                 double amount;
            //                 char response[4096] = {0}; // Clear buffer for response
            //                 size_t response_len = 0;

            //                 response_len += snprintf(response + response_len, sizeof(response) - response_len,
            //                                          "LoanID | AccountNumber | Amount | Status | Assigned Employee\n");
            //                 response_len += snprintf(response + response_len, sizeof(response) - response_len,
            //                                          "---------------------------------------------------------------\n");

            //                 while (fgets(line, sizeof(line), fp))
            //                 {
            //                     if (sscanf(line, "%31[^|]|%31[^|]|%lf|%31[^|]|%31s",
            //                                loan_id, account_num, &amount, status, loan_emp_id) == 5)
            //                     {
            //                         if (strcmp(loan_emp_id, employee_id) == 0)
            //                         {
            //                             response_len += snprintf(response + response_len, sizeof(response) - response_len,
            //                                                      "%s | %s | %.2lf | %s | %s\n",
            //                                                      loan_id, account_num, amount, status, loan_emp_id);
            //                             if (response_len >= sizeof(response) - 100)
            //                                 break;
            //                         }
            //                     }
            //                 }

            //                 unlock_file(fd);
            //                 fclose(fp);

            //                 if (response_len == 0)
            //                 {
            //                     const char *msg = "No loan applications assigned to you currently.\n";
            //                     send(new_socket, msg, strlen(msg), 0);
            //                 }
            //                 else
            //                 {
            //                     send(new_socket, response, response_len, 0);
            //                 }

            //                 // send(new_socket, emp_menu, strlen(emp_menu), 0);
            //                 break;
            //             }

            // case 4: // Reject Loan
            // {
            //     char employeeid[16] = {0};
            //     char loan_id_to_reject[32];
            //     int res;

            //     // Open and find employee ID using logged_in_username
            //     FILE *fp_emp = fopen(EMPLOYEE_FILE, "r");
            //     if (!fp_emp)
            //     {
            //         const char *msg = "Error opening employee data file.\n";
            //         send(new_socket, msg, strlen(msg), 0);
            //         break;
            //     }

            //     char line[256], uname[64], id[16], mobile[16];
            //     while (fgets(line, sizeof(line), fp_emp))
            //     {
            //         if (sscanf(line, "%15[^|]|%63[^|]|%15[^\n]", id, uname, mobile) == 3)
            //         {
            //             trim(uname);
            //             if (strcmp(uname, logged_in_username) == 0)
            //             {
            //                 strcpy(employeeid, id);
            //                 break;
            //             }
            //         }
            //     }
            //     fclose(fp_emp);

            //     if (employeeid[0] == '\0')
            //     {
            //         const char *msg = "Employee ID not found for user.\n";
            //         send(new_socket, msg, strlen(msg), 0);
            //         break;
            //     }

            //     // Prompt client to enter loan ID to reject
            //     const char *prompt = "Enter loan ID to reject:\n";
            //     send(new_socket, prompt, strlen(prompt), 0);

            //     // Receive loan ID from client
            //     int valread = recv(new_socket, loan_id_to_reject, sizeof(loan_id_to_reject) - 1, 0);
            //     if (valread <= 0)
            //         break;
            //     loan_id_to_reject[valread] = '\0';
            //     trim(loan_id_to_reject);

            //     // Call reject function
            //     res = reject_loan_by_id(loan_id_to_reject, employeeid);

            //     // Send response based on reject function result
            //     if (res == 0)
            //     {
            //         const char *msg = "Loan rejected successfully.\n";
            //         send(new_socket, msg, strlen(msg), 0);
            //     }
            //     else if (res == 1)
            //     {
            //         const char *msg = "Loan ID not found.\n";
            //         send(new_socket, msg, strlen(msg), 0);
            //     }
            //     else if (res == 2)
            //     {
            //         const char *msg = "Loan not assigned to you.\n";
            //         send(new_socket, msg, strlen(msg), 0);
            //     }

            //     // Send employee menu prompt to client for continued interaction

            //     break;
            // }
            //             case 5:
            //             {
            //                 char buffer[1024];
            //                 char loan_id_to_approve[256];
            //                 int valread, res;
            //                 char employeeid[16] = {0};

            //                 // Retrieve or have employeeid from logged-in session/context
            //                 // Example: assume a function to get employee ID by username
            //                 FILE *fp = fopen(EMPLOYEE_FILE, "r");
            //                 if (!fp)
            //                 {
            //                     const char *msg = "Error opening employee data file.\n";
            //                     send(new_socket, msg, strlen(msg), 0);
            //                     break;
            //                 }
            //                 char line[256], uname[64], id[16], mobile[16];
            //                 while (fgets(line, sizeof(line), fp))
            //                 {
            //                     if (sscanf(line, "%15[^|]|%63[^|]|%15[^\n]", id, uname, mobile) == 3)
            //                     {
            //                         trim(uname);
            //                         if (strcmp(uname, logged_in_username) == 0)
            //                         {
            //                             strcpy(employeeid, id);
            //                             break;
            //                         }
            //                     }
            //                 }
            //                 fclose(fp);
            //                 if (employeeid[0] == '\0')
            //                 {
            //                     const char *msg = "Employee ID not found for user.\n";
            //                     send(new_socket, msg, strlen(msg), 0);
            //                     break;
            //                 }

            //                 // Receive loan ID from client (no prompt sent by server)
            //                 valread = recv(new_socket, loan_id_to_approve, sizeof(loan_id_to_approve) - 1, 0);
            //                 if (valread <= 0)
            //                     break;
            //                 loan_id_to_approve[valread] = '\0';
            //                 trim(loan_id_to_approve);

            //                 // Call the approve loan function
            //                 res = approve_loan_by_id(loan_id_to_approve, employeeid);

            //                 // Set response message based on function result
            //                 if (res == 0)
            //                 {
            //                     strcpy(buffer, "Loan approved successfully.\n");
            //                 }
            //                 else if (res == 1)
            //                 {
            //                     strcpy(buffer, "Loan ID not found.\n");
            //                 }
            //                 else if (res == 2)
            //                 {
            //                     strcpy(buffer, "Loan not assigned to you.\n");
            //                 }
            //                 else
            //                 {
            //                     strcpy(buffer, "Unknown error.\n");
            //                 }

            //                 // Send response message
            //                 send(new_socket, buffer, strlen(buffer), 0);

            //                 // Send employee menu prompt again
            //                 send(new_socket, emp_menu, strlen(emp_menu), 0);

            //                 break;
            //             }
            //             case 6:
            //                 // view_assigned_loan_applications(new_socket);
            //                 break;
            // case 7: // Change Password
            // {
            //     char buffer[1024];
            //     char new_password[256];
            //     int valread;

            //     // Prompt client to enter a new password or just wait for input (no prompt sent)
            //     // Client should print prompt locally

            //     // Receive new password from client
            //     valread = recv(new_socket, new_password, sizeof(new_password) - 1, 0);
            //     if (valread <= 0)
            //         break;
            //     new_password[valread] = '\0';
            //     trim(new_password);

            //     // Call your employee password change function
            //     bool success = employee_change_password(logged_in_username, new_password);

            //     if (success)
            //     {
            //         strcpy(buffer, "Password changed successfully.\n");
            //     }
            //     else
            //     {
            //         strcpy(buffer, "Failed to change password.\n");
            //     }

            //     // Send operation result to client
            //     send(new_socket, buffer, strlen(buffer), 0);

            //     // Send employee menu prompt again for further actions
            //     send(new_socket, emp_menu, strlen(emp_menu), 0);

            //     break;
            // }

            //             case 8:
            //             {
            //                 // Assume username stored per client session
            //                 handle_user_logout(username);

            //                 // Send logout success message
            //                 char *msg = "Logged out successfully.\n";
            //                 send(new_socket, msg, strlen(msg), 0);

            //                 // Close socket and terminate thread/session handling below
            //                 close(new_socket);
            //                 pthread_exit(NULL);
            //             }
            //             default:
            //                 strcpy(response, "Invalid choice. Please try again.\n");
            //                 send(new_socket, response, strlen(response), 0);
            //                 break;
            //             }
            //             memset(buffer, 0, sizeof(buffer));
            //             memset(response, 0, sizeof(response));
            //         }
            //     }
            //     else
            //     {
            //         printf("DEBUG: Unknown action: %s\n", action);
            //         fp = fopen("users.txt", "a+");
            //         if (!fp)
            //         {
            //             strcpy(response, "Server error.\n");
            //             send(new_socket, response, strlen(response), 0);
            //             write_server_log(client_ip, buffer, response);
            //             continue;
            //         }
            //         int found = 0;
            //         char line[256];
            //         rewind(fp);
            //         while (fgets(line, sizeof(line), fp))
            //         {
            //             char utype[16], uname[64], upwd[64];
            //             sscanf(line, "%[^|]|%[^|]|%[^|]", utype, uname, upwd);
            //             trim(utype);
            //             trim(uname);
            //             trim(upwd);
            //             if (strcmp(utype, role) == 0 && strcmp(uname, username) == 0 &&
            //                 strcmp(upwd, password) == 0)
            //             {
            //                 found = 1;
            //                 break;
            //             }
            //         }
            //         fclose(fp);
            //         if (found)
            //             strcpy(response, "Login successful.\n");
            //         else
            //             strcpy(response, "Login failed.\n");
            //         send(new_socket, response, strlen(response), 0);
            //         write_server_log(client_ip, buffer, response);
            //     }
            // }

            else if (strcmp(role, "manager") == 0)
            {
                // Validate manager login
                if (validate_manager_login(username, password))
                {
                    if (add_active_user(username) != 0)
                    {
                        char *msg = "Maximum active users reached. Try again later.\n";
                        send(new_socket, msg, strlen(msg), 0);
                        close(new_socket);
                        pthread_exit(NULL);
                    }
                    strcpy(logged_in_username, username);
                    strcpy(response, "Login successful.");
                    send(new_socket, response, strlen(response), 0);
                    write_server_log(client_ip, buffer, response);

                    // Manager menu string
                    const char *manager_menu =
                        "Manager Menu:\n"
                        "1. Activate/Deactivate Customer Accounts\n"
                        "2. Assign Loan Application Processes to Employees\n"
                        "3. Review Customer Feedback\n"
                        "4. Change Password\n"
                        "5. Logout\n"
                        "Enter choice: \n";

                    while (1)
                    {
                        printf("[DEBUG] Sending manager menu\n");
                        send(new_socket, manager_menu, strlen(manager_menu), 0);

                        int valread = recv(new_socket, buffer, sizeof(buffer) - 1, 0);
                        if (valread <= 0)
                            break;
                        buffer[valread] = '\0';
                        int choice = atoi(buffer);

                        switch (choice)
                        {
                        case 1:
                        {
                            char input[64];
                            char status_choice[16];
                            char response[256];

                            // printf("[DEBUG] Received choice 1: Activate/Deactivate Customer Account\n");

                            // Prompt for account number
                            const char *prompt1 = "Enter customer account number to activate/deactivate: ";
                            send(new_socket, prompt1, strlen(prompt1), 0);
                            // printf("[DEBUG] Sent prompt1\n");

                            int valread = recv(new_socket, input, sizeof(input) - 1, 0);
                            if (valread <= 0)
                            {
                                // printf("[DEBUG] Failed to read account number input\n");
                                break;
                            }
                            input[valread] = '\0';
                            trim(input);
                            // printf("[DEBUG] Received account number: %s\n", input);

                            // Prompt for status
                            const char *prompt2 = "Enter status (Active/Deactive): ";
                            send(new_socket, prompt2, strlen(prompt2), 0);
                            // printf("[DEBUG] Sent prompt2\n");

                            valread = recv(new_socket, status_choice, sizeof(status_choice) - 1, 0);
                            if (valread <= 0)
                            {
                                // printf("[DEBUG] Failed to read status input\n");
                                break;
                            }
                            status_choice[valread] = '\0';
                            trim(status_choice);
                            // printf("[DEBUG] Received status choice: %s\n", status_choice);

                            // Validate input and update account
                            if (strcasecmp(status_choice, "Active") != 0 && strcasecmp(status_choice, "Deactive") != 0)
                            {
                                strcpy(response, "Invalid status. Please enter 'Active' or 'Deactive'.\n");
                                send(new_socket, response, strlen(response), 0);
                                // printf("[DEBUG] Invalid status sent\n");
                                break;
                            }

                            bool success = manager_set_customer_account_status(input, status_choice);
                            if (success)
                            {
                                strcpy(response, "Customer account status updated successfully.\n");
                            }
                            else
                            {
                                strcpy(response, "Failed to update customer account status.\n");
                            }
                            send(new_socket, response, strlen(response), 0);
                            // printf("[DEBUG] Sent response: %s\n", response);

                            write_server_log(client_ip, "Activate/Deactivate Customer", response);

                            break;
                        }
                        case 2:
                        {
                            char buffer[128];
                            char loanID[32], employeeID[32];
                            int valread;

                            // Prompt for Loan ID
                            const char *loan_prompt = "Enter Loan ID: ";
                            send(new_socket, loan_prompt, strlen(loan_prompt), 0);
                            valread = read(new_socket, loanID, sizeof(loanID) - 1);
                            if (valread <= 0)
                                break;
                            loanID[valread] = '\0';
                            trim(loanID);

                            // Prompt for Employee ID
                            const char *emp_prompt = "Enter Employee ID: ";
                            send(new_socket, emp_prompt, strlen(emp_prompt), 0);
                            valread = read(new_socket, employeeID, sizeof(employeeID) - 1);
                            if (valread <= 0)
                                break;
                            employeeID[valread] = '\0';
                            trim(employeeID);

                            // Call your assignment function - returns int status code
                            // 0 = success, 1 = invalid loanID, 2 = invalid employeeID
                            int result = assign_loan_to_employee(loanID, employeeID);

                            if (result == 0)
                            {
                                send(new_socket, "0", 1, 0); // success
                            }
                            else if (result == 1)
                            {
                                send(new_socket, "1", 1, 0); // invalid loanID
                            }
                            else if (result == 2)
                            {
                                send(new_socket, "2", 1, 0); // invalid employeeID
                            }
                            else
                            {
                                send(new_socket, "3", 1, 0); // some other error
                            }
                            break;
                        }

                        case 3:
                        {
                            // Allocate buffer for feedback
                            char feedback_buffer[8192]; // adjust size as needed

                            bool success = get_all_feedback(feedback_buffer, sizeof(feedback_buffer));

                            if (success)
                            {
                                // Send the accumulated feedback to the client
                                send(new_socket, feedback_buffer, strlen(feedback_buffer), 0);
                            }
                            else
                            {
                                const char *msg = "No feedback available or failed to read feedback.\n";
                                send(new_socket, msg, strlen(msg), 0);
                            }
                            break;
                        }

                        case 4: // Manager Change Password
                        {
                            char username[64];
                            char new_password[64];
                            char response[256];

                            // Send prompt for username
                            const char *prompt1 = "Enter username to change password: ";
                            send(new_socket, prompt1, strlen(prompt1), 0);
                            int valread = recv(new_socket, username, sizeof(username) - 1, 0);
                            if (valread <= 0)
                                break;
                            username[valread] = '\0';
                            trim(username);

                            // Send prompt for new password
                            const char *prompt2 = "Enter new password: ";
                            send(new_socket, prompt2, strlen(prompt2), 0);
                            valread = recv(new_socket, new_password, sizeof(new_password) - 1, 0);
                            if (valread <= 0)
                                break;
                            new_password[valread] = '\0';
                            trim(new_password);

                            bool res = change_password(username, new_password);

                            if (res)
                                strcpy(response, "Password changed successfully.\n");
                            else
                                strcpy(response, "Failed to change password. User may not exist.\n");

                            send(new_socket, response, strlen(response), 0);
                            write_server_log(client_ip, "Manager Change Password", response);

                            break;
                        }

                        case 5: // Manager Logout
                        {
                            const char *msg = "Logged out successfully.\n";
                            send(new_socket, msg, strlen(msg), 0);
                            close(new_socket);
                            pthread_exit(NULL);
                        }

                        default:
                            send(new_socket, "Invalid choice, try again.\n", 26, 0);
                            break;
                        }
                    }
                }
                else
                {
                    strcpy(response, "Login failed.\n");
                    send(new_socket, response, strlen(response), 0);
                    write_server_log(client_ip, buffer, response);
                }
            }
            else if (strcmp(role, "admin") == 0)
            {
                // Validate admin login
                if (validate_admin_login(username, password))
                {
                    if (add_active_user(username) != 0)
                    {
                        char *msg = "Maximum active users reached. Try again later.\n";
                        send(new_socket, msg, strlen(msg), 0);
                        close(new_socket);
                        pthread_exit(NULL);
                    }
                    strcpy(logged_in_username, username);
                    strcpy(response, "Login successful.");
                    send(new_socket, response, strlen(response), 0);
                    write_server_log(client_ip, buffer, response);

                    // Admin menu string
                    const char *admin_menu =
                        "Admin Menu:\n"
                        "1. Add New Employee\n"
                        "2. Modify User Details\n"
                        "3. Change Password\n"
                        "4. Change User Role\n"
                        "5. Logout\n"
                        "Enter choice: ";

                    while (1)
                    {
                        send(new_socket, admin_menu, strlen(admin_menu), 0);

                        int valread = recv(new_socket, buffer, sizeof(buffer) - 1, 0);
                        if (valread <= 0)
                            break;
                        buffer[valread] = '\0';
                        int choice = atoi(buffer);

                        switch (choice)
                        {
                        case 1:
                        {
                            // Add New Employee
                            char new_role[16], new_username[64], new_password[64], new_mobile[32];
                            // Send and receive data for new employee details (similarly handle input prompts and trimming)
                            // Example:
                            send(new_socket, "Enter role (employee/manager): ", 30, 0);
                            recv(new_socket, new_role, sizeof(new_role) - 1, 0);
                            trim(new_role);
                            send(new_socket, "Enter username: ", 16, 0);
                            recv(new_socket, new_username, sizeof(new_username) - 1, 0);
                            trim(new_username);
                            send(new_socket, "Enter password: ", 16, 0);
                            recv(new_socket, new_password, sizeof(new_password) - 1, 0);
                            trim(new_password);
                            send(new_socket, "Enter mobile number: ", 21, 0);
                            recv(new_socket, new_mobile, sizeof(new_mobile) - 1, 0);
                            trim(new_mobile);

                            int add_result = add_new_employee(new_role, new_username, new_password, new_mobile);
                            if (add_result == 0)
                            {
                                strcpy(response, "New employee added successfully.\n");
                            }
                            else if (add_result == -2)
                            {
                                strcpy(response, "Username already exists.\n");
                            }
                            else
                            {
                                strcpy(response, "Failed to add new employee.\n");
                            }
                            send(new_socket, response, strlen(response), 0);
                            write_server_log(client_ip, "Add New Employee", response);
                            break;
                        }
                        case 2:
                        {
                            char user_type[16], username[64], new_password[64], new_mobile[32];
                            const char *prompt = "Modify which user type? (customer/employee/manager): \n";
                            send(new_socket, prompt, strlen(prompt), 0);
                            recv(new_socket, user_type, sizeof(user_type) - 1, 0);
                            trim(user_type);

                            send(new_socket, "Enter username: ", 16, 0);
                            recv(new_socket, username, sizeof(username) - 1, 0);
                            trim(username);

                            send(new_socket, "Enter new password or write 'NO' to keep unchanged: ", 53, 0);
                            recv(new_socket, new_password, sizeof(new_password) - 1, 0);
                            trim(new_password);

                            send(new_socket, "Enter new mobile number or write 'NO' to keep unchanged: ", 58, 0);
                            recv(new_socket, new_mobile, sizeof(new_mobile) - 1, 0);
                            trim(new_mobile);

                            // Check if admin typed NO to skip change
                            if (strcasecmp(new_password, "NO") == 0)
                            {
                                new_password[0] = '\0'; // Treat as no change
                            }
                            if (strcasecmp(new_mobile, "NO") == 0)
                            {
                                new_mobile[0] = '\0';
                            }

                            int mod_result = modify_user_details(user_type, username, new_password, new_mobile);
                            if (mod_result == 0)
                            {
                                strcpy(response, "User details updated successfully.\n");
                            }
                            else
                            {
                                strcpy(response, "Failed to update user details.\n");
                            }
                            send(new_socket, response, strlen(response), 0);
                            write_server_log(client_ip, "Modify User Details", response);
                            break;
                        }
                        case 3:
                        {
                            char username[64];
                            char new_password[64];
                            char response[256];

                            // Send prompt for username
                            const char *prompt1 = "Enter username to change password: ";
                            send(new_socket, prompt1, strlen(prompt1), 0);
                            int valread = recv(new_socket, username, sizeof(username) - 1, 0);
                            if (valread <= 0)
                                break;
                            username[valread] = '\0';
                            trim(username);

                            // Send prompt for new password
                            const char *prompt2 = "Enter new password: ";
                            send(new_socket, prompt2, strlen(prompt2), 0);
                            valread = recv(new_socket, new_password, sizeof(new_password) - 1, 0);
                            if (valread <= 0)
                                break;
                            new_password[valread] = '\0';
                            trim(new_password);

                            // Call your admin_change_password function
                            bool res = admin_change_password(username, new_password);

                            if (res)
                            {
                                strcpy(response, "Password changed successfully.\n");
                            }
                            else
                            {
                                strcpy(response, "Failed to change password. User may not exist.\n");
                            }

                            send(new_socket, response, strlen(response), 0);
                            write_server_log(client_ip, "Admin Change Password", response);
                            break;
                        }
                        case 4:
                        {
                            char username[64];
                            send(new_socket, "Enter username to change role: ", 30, 0);
                            recv(new_socket, username, sizeof(username) - 1, 0);
                            trim(username);

                            int change_res = change_user_role(username);

                            switch (change_res)
                            {
                            case 0:
                                strcpy(response, "User role changed successfully.\n");
                                break;
                            case -1:
                                strcpy(response, "System error occurred.\n");
                                break;
                            case -2:
                                strcpy(response, "User not found.\n");
                                break;
                            case -3:
                                strcpy(response, "Cannot change roles now: active loans exist.\n");
                                break;
                            case -4:
                                strcpy(response, "Invalid user role.\n");
                                break;
                            default:
                                strcpy(response, "Unknown error.\n");
                            }

                            send(new_socket, response, strlen(response), 0);
                            write_server_log(client_ip, "Manage User Roles", response);
                            break;
                        }
                        case 5:
                        {
                            // Assume username stored per client session
                            handle_user_logout(username);

                            // Send logout success message
                            char *msg = "Logged out successfully.\n";
                            send(new_socket, msg, strlen(msg), 0);

                            // Close socket and terminate thread/session handling below
                            close(new_socket);
                            pthread_exit(NULL);
                        }
                        default:
                            send(new_socket, "Invalid choice, try again.\n", 26, 0);
                            break;
                        }
                    }
                }
                else
                {
                    strcpy(response, "Login failed.\n");
                    send(new_socket, response, strlen(response), 0);
                    write_server_log(client_ip, buffer, response);
                }
            }
            else
            {
                strcpy(response, "Invalid request.\n");
                send(new_socket, response, strlen(response), 0);
                write_server_log(client_ip, buffer, response);
            }
            memset(buffer, 0, sizeof(buffer));
        }
        close(new_socket);
    }
}

void *thread_func(void *arg)
{
    int new_socket = *(int *)arg;
    handle_client(new_socket);
    free(arg);
    return NULL;
}

int main()
{
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 3) < 0)
    {
        perror("Listen");
        exit(EXIT_FAILURE);
    }
    printf("Server listening on port %d...\n", PORT);

    while (1)
    {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen)) < 0)
        {
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
