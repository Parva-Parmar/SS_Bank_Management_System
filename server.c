#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>

#include "customer.h"
#include "employee.h"
#include "admin.h"

#define PORT 8080
#define SERVER_LOG_FILE "server.log"
#define USERS_FILE "users.txt"
#define CUSTOMER_DATA_FILE "customer_data.txt"
#define EMPLOYEE_FILE "employee_data.txt"
#define MANAGER_FILE "manager_data.txt"

pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
char logged_in_username[64] = {0}; // store username after login

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
        if (strcmp(action, "login") == 0)
        {
            if (strcmp(role, "customer") == 0)
            {
                printf("DEBUG: Handling customer login\n");
                if (validate_customer_login(username, password))
                {
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
                            // Ask for recipient username
                            char prompt[] = "Enter recipient username: ";
                            send(new_socket, prompt, strlen(prompt), 0);

                            int len = read(new_socket, buffer, sizeof(buffer) - 1);
                            if (len <= 0)
                                break;
                            buffer[len] = '\0';
                            trim(buffer);
                            char recipient[64];
                            strcpy(recipient, buffer);

                            // Ask for transfer amount
                            strcpy(prompt, "Enter transfer amount: ");
                            send(new_socket, prompt, strlen(prompt), 0);

                            len = read(new_socket, buffer, sizeof(buffer) - 1);
                            if (len <= 0)
                                break;
                            buffer[len] = '\0';
                            float amount = atof(buffer);

                            bool success = transfer_funds(username, recipient, amount);

                            if (success)
                                strcpy(response, "Transfer successful.\n");
                            else
                                strcpy(response, "Transfer failed. Check recipient or insufficient funds.\n");

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
                }

                else
                {
                    printf("DEBUG: Employee login failed.\n");
                    strcpy(response, "Login failed.\n");
                    send(new_socket, response, strlen(response), 0);
                    write_server_log(client_ip, buffer, response);
                }
            }
            else if (strcmp(role, "employee") == 0)
            {
                printf("here\n");
                printf("DEBUG: Entered employee login branch for user %s\n", username);
                int valid = validate_employee_login(username, password);
                printf("DEBUG: validate_employee_login returned %d\n", valid);
                if (valid)
                {
                    // After verifying user credentials
                    strcpy(logged_in_username, username);
                    printf("Debug: logged_in_username = '%s'\n", logged_in_username);
                    printf("DEBUG: Employee login successful.\n");
                    strcpy(response, "Login successful.\n");
                    send(new_socket, response, strlen(response), 0);
                    write_server_log(client_ip, buffer, response);

                    // Employee menu string
                    char emp_menu[] =
                        "Employee Menu:\n"
                        "1. Add New Customer\n"
                        "2. Modify Customer Details\n"
                        "3. Approve Loan\n"
                        "4. Reject Loan\n"
                        "5. View Assigned Loan Applications\n"
                        "6. View Customer Transactions\n"
                        "7. Change Password\n"
                        "8. Logout\n"
                        "Enter choice: \n";

                    while (1)
                    {
                        send(new_socket, emp_menu, strlen(emp_menu), 0);
                        valread = read(new_socket, buffer, sizeof(buffer) - 1);
                        if (valread <= 0)
                            break;
                        buffer[valread] = '\0';
                        int choice = atoi(buffer);
                        char employee_id[16] = {0};
                        FILE *fp = fopen("employee_data.txt", "r");
                        if (!fp)
                        {
                            send(new_socket, "Error opening employee data file.\n", 34, 0);
                            break;
                        }
                        char line[256];
                        char username[64];
                        strcpy(username, logged_in_username); // Use your logged-in username storage
                        while (fgets(line, sizeof(line), fp))
                        {
                            char id[16], uname[64], mobile[32];
                            if (sscanf(line, "%15[^|]|%63[^|]|%31[^\n]", id, uname, mobile) == 3)
                            {
                                if (strcmp(uname, username) == 0)
                                {
                                    strcpy(employee_id, id);
                                    break;
                                }
                            }
                        }
                        fclose(fp);

                        if (employee_id[0] == '\0')
                        {
                            send(new_socket, "Employee ID not found for user.\n", 32, 0);
                            break;
                        }
                        printf("Debug: emp_id = '%s'\n", employee_id);
                        switch (choice)
                        {
                        case 1:
                        {
                            char add_new_username[64], add_new_password[64], add_new_mobile[16], account_number[16];
                            FILE *fp;
                            long max_account_num = 1000000;

                            send(new_socket, "Enter new customer username:\n", 29, 0);
                            valread = read(new_socket, add_new_username, sizeof(add_new_username) - 1);
                            if (valread <= 0)
                                break;
                            add_new_username[valread] = '\0';
                            trim(add_new_username);

                            send(new_socket, "Enter password:\n", 16, 0);
                            valread = read(new_socket, add_new_password, sizeof(add_new_password) - 1);
                            if (valread <= 0)
                                break;
                            add_new_password[valread] = '\0';
                            trim(add_new_password);

                            send(new_socket, "Enter mobile number:\n", 21, 0);
                            valread = read(new_socket, add_new_mobile, sizeof(add_new_mobile) - 1);
                            if (valread <= 0)
                                break;
                            add_new_mobile[valread] = '\0';
                            trim(add_new_mobile);

                            // Generate unique account number
                            fp = fopen(CUSTOMER_DATA_FILE, "r");
                            if (fp)
                            {
                                char line[256];
                                while (fgets(line, sizeof(line), fp))
                                {
                                    char username[64], mobile[16], loan_status[32];
                                    float balance;
                                    long acct_num;
                                    if (sscanf(line, "%[^|]|%ld|%[^|]|%f|%s", username, &acct_num, mobile, &balance, loan_status) == 5)
                                    {
                                        if (acct_num >= max_account_num)
                                            max_account_num = acct_num + 1;
                                    }
                                }
                                fclose(fp);
                            }
                            snprintf(account_number, sizeof(account_number), "%ld", max_account_num);

                            // Call add_new_customer with full details
                            int result = add_new_customer(add_new_username, add_new_password, account_number, add_new_mobile);

                            if (result == 0)
                                send(new_socket, "New customer added successfully.\n", 32, 0);
                            else if (result == -2)
                                send(new_socket, "Username already exists. Try another.\n", 38, 0);
                            else
                                send(new_socket, "Failed to add new customer.\n", 27, 0);

                            send(new_socket, emp_menu, strlen(emp_menu), 0);
                            break;
                        }
                        case 2:
                            modify_customer_details(new_socket);
                            break;
                        case 3:
                        {
                            process_loan_applications1(new_socket, employee_id);
                            break;
                        }
                        case 4:
                            process_loan_applications2(new_socket, employee_id);
                            break;
                        case 5:
                            // reject_loan(new_socket, employee_id);
                            break;
                        case 6:
                            // view_assigned_loan_applications(new_socket);
                            break;
                        case 7:
                            // view_customer_transactions(new_socket);
                            break;
                        case 8:
                            // change_password(new_socket);
                            break;
                        case 9:
                            strcpy(response, "Logging out...\n");
                            send(new_socket, response, strlen(response), 0);
                            write_server_log(client_ip, "Logout", response);
                            return; // Exit loop and finish session for employee
                        default:
                            strcpy(response, "Invalid choice. Please try again.\n");
                            send(new_socket, response, strlen(response), 0);
                            break;
                        }
                        memset(buffer, 0, sizeof(buffer));
                        memset(response, 0, sizeof(response));
                    }
                }
                else
                {
                    printf("DEBUG: Unknown action: %s\n", action);
                    fp = fopen("users.txt", "a+");
                    if (!fp)
                    {
                        strcpy(response, "Server error.\n");
                        send(new_socket, response, strlen(response), 0);
                        write_server_log(client_ip, buffer, response);
                        continue;
                    }
                    int found = 0;
                    char line[256];
                    rewind(fp);
                    while (fgets(line, sizeof(line), fp))
                    {
                        char utype[16], uname[64], upwd[64];
                        sscanf(line, "%[^|]|%[^|]|%[^|]", utype, uname, upwd);
                        trim(utype);
                        trim(uname);
                        trim(upwd);
                        if (strcmp(utype, role) == 0 && strcmp(uname, username) == 0 &&
                            strcmp(upwd, password) == 0)
                        {
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
            }
            else if (strcmp(role, "admin") == 0)
            {
                // Validate admin login
                if (validate_admin_login(username, password))
                {
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
                            // Logout admin
                            strcpy(response, "Logging out...\n");
                            send(new_socket, response, strlen(response), 0);
                            write_server_log(client_ip, "Logout", response);
                            return; // End admin session
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
        else if (strcmp(action, "signup") == 0)
        {
            // printf("DEBUG: Handling signup for role %s\n", role);

            // Parse signup with mobile included
            // Format expected: "signup|role|username|password|mobile"
            int parsed = sscanf(buffer, "%[^|]|%[^|]|%[^|]|%[^|]|%[^|]", action, role, username, password, mobile);

            if (parsed != 5)
            { // All five fields should be parsed after 'signup'
                strcpy(response, "Invalid signup data format.\n");
                send(new_socket, response, strlen(response), 0);
                write_server_log(client_ip, buffer, response);
                continue;
            }

            char id[16];
            char data_filename[64];
            FILE *data_fp;

            if (strcmp(role, "employee") == 0)
            {
                data_fp = fopen("employee_data.txt", "r");
                if (!data_fp)
                {
                    snprintf(id, sizeof(id), "E000001");
                }
                else
                {
                    char line[256];
                    int max_num = 0;
                    while (fgets(line, sizeof(line), data_fp))
                    {
                        char curr_id[16];
                        sscanf(line, "%15[^|]", curr_id);
                        if (curr_id[0] == 'E')
                        {
                            int num = atoi(curr_id + 1);
                            if (num > max_num)
                                max_num = num;
                        }
                    }
                    fclose(data_fp);
                    snprintf(id, sizeof(id), "E%06d", max_num + 1);
                }
                strcpy(data_filename, "employee_data.txt");
            }
            else if (strcmp(role, "manager") == 0)
            {
                data_fp = fopen("manager_data.txt", "r");
                if (!data_fp)
                {
                    snprintf(id, sizeof(id), "M000001");
                }
                else
                {
                    char line[256];
                    int max_num = 0;
                    while (fgets(line, sizeof(line), data_fp))
                    {
                        char curr_id[16];
                        sscanf(line, "%15[^|]", curr_id);
                        if (curr_id[0] == 'M')
                        {
                            int num = atoi(curr_id + 1);
                            if (num > max_num)
                                max_num = num;
                        }
                    }
                    fclose(data_fp);
                    snprintf(id, sizeof(id), "M%06d", max_num + 1);
                }
                strcpy(data_filename, "manager_data.txt");
            }
            else
            {
                strcpy(response, "Invalid role for signup.\n");
                send(new_socket, response, strlen(response), 0);
                write_server_log(client_ip, buffer, response);
                continue;
            }

            // Append user to users.txt
            FILE *fp = fopen("users.txt", "a+");
            if (!fp)
            {
                strcpy(response, "Server error opening users file.\n");
                send(new_socket, response, strlen(response), 0);
                write_server_log(client_ip, buffer, response);
                continue;
            }
            fprintf(fp, "%s|%s|%s\n", role, username, password);
            fclose(fp);

            // Append role data to employee_data.txt or manager_data.txt
            data_fp = fopen(data_filename, "a+");
            if (!data_fp)
            {
                strcpy(response, "Server error opening role data file.\n");
                send(new_socket, response, strlen(response), 0);
                write_server_log(client_ip, buffer, response);
                continue;
            }
            fprintf(data_fp, "%s|%s|%s\n", id, username, mobile);
            fclose(data_fp);

            snprintf(response, sizeof(response), "Signup successful. Your ID is %s\n", id);
            send(new_socket, response, strlen(response), 0);
            write_server_log(client_ip, buffer, response);
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
