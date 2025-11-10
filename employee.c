#include "employee.h"
 

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/types.h>  // For off_t and other types
#include <sys/file.h>   // For flock
#include <sys/socket.h> // For send, read and sockets
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include "filelock.h"
#include "utils.h"
#include "filelock.h"

#define USERS_FILE "users.txt"
#define CUSTOMER_DATA_FILE "customer_data.txt"
void trim(char *str);

int validate_employee_login(const char *username, const char *password)
{
    FILE *fp = fopen(USERS_FILE, "r");
    if (!fp)
    {
        return 0; // Cannot open file, treat as login failure
    }

    char line[256];
    while (fgets(line, sizeof(line), fp))
    {
        char role[32], file_user[64], file_pass[64];
        if (sscanf(line, "%31[^|]|%63[^|]|%63[^\n]", role, file_user, file_pass) == 3)
        {
            if (strcmp(role, "employee") == 0 &&
                strcmp(file_user, username) == 0 &&
                strcmp(file_pass, password) == 0)
            {
                fclose(fp);
                return 1; // Valid employee credentials
            }
        }
    }
    fclose(fp);
    return 0; // No valid match found
}

static FILE *open_file_lockedd(const char *filename, const char *mode, int *fd_out) {
    FILE *fp = fopen(filename, mode);
    if (!fp)
        return NULL;
    int fd = fileno(fp);
    if (flock(fd, LOCK_EX) != 0) {
        fclose(fp);
        return NULL;
    }
    if (fd_out)
        *fd_out = fd;
    return fp;
}

static void close_file_lockedd(FILE *fp, int fd) {
    flock(fd, LOCK_UN);
    fclose(fp);
}
int add_new_customer(const char *username, const char *password, const char *account_number, const char *mobile)
{
    char buffer[256];

    // Check if username exists in USERS_FILE (read lock)
    FILE *fp = fopen(USERS_FILE, "r");
    if (!fp)
        return -1;

    int fd = fileno(fp);
    if (lock_file(fd, F_RDLCK) < 0)
    {
        fclose(fp);
        return -1;
    }

    while (fgets(buffer, sizeof(buffer), fp))
    {
        char role[32], file_user[64], file_pass[64];
        sscanf(buffer, "%31[^|]|%63[^|]|%63[^\n]", role, file_user, file_pass);
        if (strcmp(file_user, username) == 0)
        {
            unlock_file(fd);
            fclose(fp);
            return -2; // Username exists
        }
    }

    unlock_file(fd);
    fclose(fp);

    // Append to USERS_FILE (write lock)
    fp = fopen(USERS_FILE, "a");
    if (!fp)
        return -1;

    fd = fileno(fp);
    if (lock_file(fd, F_WRLCK) < 0)
    {
        fclose(fp);
        return -1;
    }

    fprintf(fp, "customer|%s|%s\n", username, password);

    fflush(fp);
    unlock_file(fd);
    fclose(fp);

    // Append full detailed customer data (write lock)
    fp = fopen(CUSTOMER_DATA_FILE, "a");
    if (!fp)
        return -1;

    fd = fileno(fp);
    if (lock_file(fd, F_WRLCK) < 0)
    {
        fclose(fp);
        return -1;
    }

    fprintf(fp, "%s|%s|%s|%.2f|%s\n", username, account_number, mobile, 0.00, "Active");

    fflush(fp);
    unlock_file(fd);
    fclose(fp);

    return 0;
}

int modify_customer_details(const char *username, const char *new_password, const char *new_mobile)
{
    bool password_updated = false;
    bool mobile_updated = false;

    // Update password in users.txt if new_password is not empty
    if (new_password && strlen(new_password) > 0)
    {
        FILE *fp = fopen("users.txt", "r+");
        if (!fp)
            return -1;

        int fd = fileno(fp);
        if (lock_file(fd, F_WRLCK) < 0)
        {
            fclose(fp);
            return -1;
        }

        char line[256], role[32], user[64], pass[64];
        long pos = 0;
        while (fgets(line, sizeof(line), fp))
        {
            if (sscanf(line, "%31[^|]|%63[^|]|%63[^\n]", role, user, pass) == 3)
            {
                trim(user);
                if (strcmp(user, username) == 0)
                {
                    fseek(fp, pos, SEEK_SET);
                    fprintf(fp, "%s|%s|%s\n", role, user, new_password);
                    fflush(fp);
                    password_updated = true;
                    break;
                }
            }
            pos = ftell(fp);
        }

        unlock_file(fd);
        fclose(fp);

        if (!password_updated)
            return -1; // User not found for password update
    }

    // Update mobile number in customer_data.txt if new_mobile is not empty
    if (new_mobile && strlen(new_mobile) > 0)
    {
        FILE *fp = fopen("customer_data.txt", "r+");
        if (!fp)
            return -1;

        int fd = fileno(fp);
        if (lock_file(fd, F_WRLCK) < 0)
        {
            fclose(fp);
            return -1;
        }

        char line[256], file_user[64], account_num[32], mobile[32], status[32];
        float balance;
        long pos = 0;
        bool found = false;

        while (fgets(line, sizeof(line), fp))
        {
            if (sscanf(line, "%63[^|]|%31[^|]|%31[^|]|%f|%31s",
                       file_user, account_num, mobile, &balance, status) == 5)
            {
                trim(file_user);
                if (strcmp(file_user, username) == 0)
                {
                    found = true;
                    fseek(fp, pos, SEEK_SET);
                    // Write new line with updated mobile but keep rest same
                    fprintf(fp, "%s|%s|%s|%.2f|%s\n", file_user, account_num, new_mobile, balance, status);
                    fflush(fp);
                    break;
                }
            }
            pos = ftell(fp);
        }

        unlock_file(fd);
        fclose(fp);

        if (!found)
            return -1; // User not found for mobile update
        mobile_updated = true;
    }

    // Success if either password or mobile updated or no updates requested (treated success)
    if ((new_password && strlen(new_password) > 0 && !password_updated) ||
        (new_mobile && strlen(new_mobile) > 0 && !mobile_updated))
        return -1;

    return 0; // Success
}

int reject_loan_by_id(const char *loan_id_to_reject, const char *username)
{
    if (!loan_id_to_reject || !username)
        return 1; // treat invalid input as failure

    // Step 1: Read employeeid for username from employee_data.txt
    char employeeid[32] = {0};
    FILE *fp_emp = fopen("employee_data.txt", "r");
    if (!fp_emp)
        return 1;

    int fd_emp = fileno(fp_emp);
    if (lock_file(fd_emp, F_RDLCK) < 0)
    {
        fclose(fp_emp);
        return 1;
    }

    char line[256], file_empid[32], file_username[64], mobile[32];
    int found_emp = 0;
    while (fgets(line, sizeof(line), fp_emp))
    {
        if (sscanf(line, "%31[^|]|%63[^|]|%31[^\n]", file_empid, file_username, mobile) == 3)
        {
            trim(file_username);
            if (strcmp(file_username, username) == 0)
            {
                strncpy(employeeid, file_empid, sizeof(employeeid) - 1);
                employeeid[sizeof(employeeid) - 1] = '\0';
                found_emp = 1;
                break;
            }
        }
    }

    unlock_file(fd_emp);
    fclose(fp_emp);

    if (!found_emp)
    {
        return 1;
    }

    // Step 2: Check and update loan status in loans.txt
    FILE *fp = fopen("loans.txt", "r+");
    if (!fp)
        return 1;

    int fd = fileno(fp);
    if (lock_file(fd, F_WRLCK) < 0)
    {
        fclose(fp);
        return 1;
    }

    long pos;
    int found = 0;
    int res = 1; // default to not found/failure

    while ((pos = ftell(fp)), fgets(line, sizeof(line), fp))
    {
        char loan_id[32], account_num[32], status[32], emp_id[32];
        double amount;

        if (sscanf(line, "%31[^|]|%31[^|]|%lf|%31[^|]|%31s",
                   loan_id, account_num, &amount, status, emp_id) == 5)
        {
            trim(loan_id);
            trim(emp_id);
            trim(status);

            if (strcmp(loan_id, loan_id_to_reject) == 0)
            {
                found = 1;

                if (strcmp(emp_id, employeeid) != 0)
                {
                    res = 2; // Loan not assigned to this employee
                }
                else if (strcasecmp(status, "Approved") == 0)
                {
                    res = 3; // Loan already approved; cannot reject
                }
                else
                {
                    // Update status to Rejected only if not approved
                    fseek(fp, pos, SEEK_SET);
                    fprintf(fp, "%s|%s|%.2lf|Rejected|%s\n", loan_id, account_num, amount, emp_id);
                    fflush(fp);
                    res = 0; // Success
                }
                break;
            }
        }
    }

    unlock_file(fd);
    fclose(fp);

    if (!found)
        res = 1;

    return res;
}

int approve_loan_by_id(const char *loan_id_to_approve, const char *username)
{
    if (!loan_id_to_approve || !username)
        return 1;

    // Step 1: Get employeeid for username
    char employeeid[32] = {0};
    FILE *fp_emp = fopen("employee_data.txt", "r");
    if (!fp_emp)
        return 1;

    int fd_emp = fileno(fp_emp);
    if (lock_file(fd_emp, F_RDLCK) < 0)
    {
        fclose(fp_emp);
        return 1;
    }

    char line[256], file_empid[32], file_username[64], mobile[32];
    int found_emp = 0;
    while (fgets(line, sizeof(line), fp_emp))
    {
        if (sscanf(line, "%31[^|]|%63[^|]|%31[^\n]", file_empid, file_username, mobile) == 3)
        {
            trim(file_username);
            if (strcmp(file_username, username) == 0)
            {
                strncpy(employeeid, file_empid, sizeof(employeeid) - 1);
                employeeid[sizeof(employeeid) - 1] = '\0';
                found_emp = 1;
                break;
            }
        }
    }
    unlock_file(fd_emp);
    fclose(fp_emp);
    if (!found_emp)
        return 1;

    // Step 2: Open loans.txt and update loan status
    FILE *fp_loans = fopen("loans.txt", "r+");
    if (!fp_loans)
        return 1;

    int fd_loans = fileno(fp_loans);
    if (lock_file(fd_loans, F_WRLCK) < 0)
    {
        fclose(fp_loans);
        return 1;
    }

    long pos;
    int found_loan = 0;
    int res = 1; // default failure
    double loan_amount = 0.0;
    char account_num[32];
    char emp_id[32];
    char status[32];

    while ((pos = ftell(fp_loans)), fgets(line, sizeof(line), fp_loans))
    {
        char loan_id[32];
        double amount;
        if (sscanf(line, "%31[^|]|%31[^|]|%lf|%31[^|]|%31s",
                   loan_id, account_num, &amount, status, emp_id) == 5)
        {
            trim(loan_id);
            trim(emp_id);
            trim(status);

            if (strcmp(loan_id, loan_id_to_approve) == 0)
            {
                found_loan = 1;

                if (strcmp(emp_id, employeeid) != 0)
                {
                    res = 2; // Loan not assigned to this employee
                }
                else if (strcasecmp(status, "Approved") == 0)
                {
                    res = 3; // Already approved
                }
                else
                {
                    loan_amount = amount;
                    // Update status to Approved
                    fseek(fp_loans, pos, SEEK_SET);
                    fprintf(fp_loans, "%s|%s|%.2lf|Approved|%s\n", loan_id, account_num, amount, emp_id);
                    fflush(fp_loans);
                    res = 0; // Success
                }
                break;
            }
        }
    }

    unlock_file(fd_loans);
    fclose(fp_loans);
    if (!found_loan)
        return 1; // loan not found

    if (res != 0)
        return res; // if loan not approved update, return error code

    // Step 3: Update customer balance
    FILE *fp_cust = fopen("customer_data.txt", "r+");
    if (!fp_cust)
        return 1;

    int fd_cust = fileno(fp_cust);
    if (lock_file(fd_cust, F_WRLCK) < 0)
    {
        fclose(fp_cust);
        return 1;
    }

    found_loan = 0;
    while ((pos = ftell(fp_cust)), fgets(line, sizeof(line), fp_cust))
    {
        char cust_username[64], cust_acc_num[32], mob[32], cust_status[32];
        double balance;
        if (sscanf(line, "%63[^|]|%31[^|]|%31[^|]|%lf|%31s",
                   cust_username, cust_acc_num, mob, &balance, cust_status) == 5)
        {
            trim(cust_acc_num);
            if (strcmp(cust_acc_num, account_num) == 0)
            {
                found_loan = 1;
                balance += loan_amount;

                // Update customer's balance line
                fseek(fp_cust, pos, SEEK_SET);
                fprintf(fp_cust, "%s|%s|%s|%.2lf|%s\n", cust_username, cust_acc_num, mob, balance, cust_status);
                fflush(fp_cust);
                break;
            }
        }
    }

    unlock_file(fd_cust);
    fclose(fp_cust);

    if (!found_loan)
        return 1;

    return 0;
}
bool employee_change_password(const char *username, const char *new_password) {
    int fd;
    FILE *fp = open_file_lockedd("users.txt", "r+", &fd);
    if (!fp) return false;

    char line[256];
    long pos = 0;
    bool updated = false;

    while (fgets(line, sizeof(line), fp)) {
        char role[32], user[64], pass[64];
        if (sscanf(line, "%31[^|]|%63[^|]|%63[^\n]", role, user, pass) == 3) {
            trim(user);
            if (strcmp(user, username) == 0) {
                fseek(fp, pos, SEEK_SET);
                fprintf(fp, "%s|%s|%s\n", role, user, new_password);
                fflush(fp);
                updated = true;
                break;
            }
        }
        pos = ftell(fp);
    }

    close_file_lockedd(fp, fd);
    return updated;
}


void process_loan_applications1(int new_socket, const char *employee_id)
{
    // Disable Nagle's algorithm for immediate send
    int flag = 1;
    setsockopt(new_socket, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag));

    // Load and send the list of loans
    FILE *fp = fopen("loans.txt", "r");
    if (!fp)
    {
        send(new_socket, "Error opening loans file.\n", 26, 0);
        return;
    }

    char line[512], list_buffer[4096];
    snprintf(list_buffer, sizeof(list_buffer), "Loan Applications assigned to you:\n");
    size_t len = strlen(list_buffer);

    while (fgets(line, sizeof(line), fp))
    {
        char loanID[32], username[64], status[32], empID[16];
        double amount;
        if (sscanf(line, "%[^|]|%[^|]|%lf|%[^|]|%s", loanID, username, &amount, status, empID) == 5)
        {
            if (strcmp(empID, employee_id) == 0)
            {
                if (len + strlen(line) + 2 < sizeof(list_buffer))
                {
                    strcat(list_buffer, line);
                    strcat(list_buffer, "\n");
                    len += strlen(line) + 1;
                }
            }
        }
    }
    fclose(fp);

    if (len == strlen("Loan Applications assigned to you:\n"))
    {
        strcat(list_buffer, "No loan applications found.\n");
    }

    // Send loan list
    send(new_socket, list_buffer, strlen(list_buffer), 0);

    // Send prompt
    const char *prompt = "Enter loan ID to process or 'back' to return:\n";
    send(new_socket, prompt, strlen(prompt), 0);

    // Wait for client's input
    char buffer[128];
    int rlen = read(new_socket, buffer, sizeof(buffer) - 1);
    if (rlen <= 0)
        return;
    buffer[rlen] = '\0';
    buffer[strcspn(buffer, "\n")] = 0; // trim newline

    if (strcmp(buffer, "back") == 0)
    {
        send(new_socket, "Returned to main menu.\n", 23, 0);
        return;
    }

    // Update loan status
    FILE *in_fp = fopen("loans.txt", "r");
    FILE *out_fp = fopen("loans.tmp", "w");
    int found = 0;
    while (fgets(line, sizeof(line), in_fp))
    {
        char loanID[32], username[64], status[32], empID[16];
        double amount;
        if (sscanf(line, "%[^|]|%[^|]|%lf|%[^|]|%s", loanID, username, &amount, status, empID) == 5)
        {
            if (strcmp(loanID, buffer) == 0)
            {
                fprintf(out_fp, "%s|%s|%.2lf|%s|%s\n", loanID, username, amount, "Approved", empID);
                found = 1;
            }
            else
            {
                fputs(line, out_fp);
            }
        }
        else
        {
            fputs(line, out_fp);
        }
    }

    fclose(in_fp);
    fclose(out_fp);

    if (!found)
    {
        send(new_socket, "Loan not found.\n", 15, 0);
        remove("loans.tmp");
        return;
    }

    rename("loans.tmp", "loans.txt");
    send(new_socket, "Loan marked for processing.\n", 27, 0);
}

void process_loan_applications2(int new_socket, const char *employee_id)
{
    // Disable Nagle's algorithm for immediate send
    int flag = 1;
    setsockopt(new_socket, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag));

    // Load and send the list of loans
    FILE *fp = fopen("loans.txt", "r");
    if (!fp)
    {
        send(new_socket, "Error opening loans file.\n", 26, 0);
        return;
    }

    char line[512], list_buffer[4096];
    snprintf(list_buffer, sizeof(list_buffer), "Loan Applications assigned to you:\n");
    size_t len = strlen(list_buffer);

    while (fgets(line, sizeof(line), fp))
    {
        char loanID[32], username[64], status[32], empID[16];
        double amount;
        if (sscanf(line, "%[^|]|%[^|]|%lf|%[^|]|%s", loanID, username, &amount, status, empID) == 5)
        {
            if (strcmp(empID, employee_id) == 0)
            {
                if (len + strlen(line) + 2 < sizeof(list_buffer))
                {
                    strcat(list_buffer, line);
                    strcat(list_buffer, "\n");
                    len += strlen(line) + 1;
                }
            }
        }
    }
    fclose(fp);

    if (len == strlen("Loan Applications assigned to you:\n"))
    {
        strcat(list_buffer, "No loan applications found.\n");
    }

    // Send loan list
    send(new_socket, list_buffer, strlen(list_buffer), 0);

    // Send prompt
    const char *prompt = "Enter loan ID to process or 'back' to return:\n";
    send(new_socket, prompt, strlen(prompt), 0);

    // Wait for client's input
    char buffer[128];
    int rlen = read(new_socket, buffer, sizeof(buffer) - 1);
    if (rlen <= 0)
        return;
    buffer[rlen] = '\0';
    buffer[strcspn(buffer, "\n")] = 0; // trim newline

    if (strcmp(buffer, "back") == 0)
    {
        send(new_socket, "Returned to main menu.\n", 23, 0);
        return;
    }

    // Update loan status
    FILE *in_fp = fopen("loans.txt", "r");
    FILE *out_fp = fopen("loans.tmp", "w");
    int found = 0;
    while (fgets(line, sizeof(line), in_fp))
    {
        char loanID[32], username[64], status[32], empID[16];
        double amount;
        if (sscanf(line, "%[^|]|%[^|]|%lf|%[^|]|%s", loanID, username, &amount, status, empID) == 5)
        {
            if (strcmp(loanID, buffer) == 0)
            {
                fprintf(out_fp, "%s|%s|%.2lf|%s|%s\n", loanID, username, amount, "Rejected", empID);
                found = 1;
            }
            else
            {
                fputs(line, out_fp);
            }
        }
        else
        {
            fputs(line, out_fp);
        }
    }

    fclose(in_fp);
    fclose(out_fp);

    if (!found)
    {
        send(new_socket, "Loan not found.\n", 15, 0);
        remove("loans.tmp");
        return;
    }

    rename("loans.tmp", "loans.txt");
    send(new_socket, "Loan marked for processing.\n", 27, 0);
}

// approve
void approve_loan(int new_socket, const char *employee_id)
{
    // Disable Nagle's algorithm for immediate send
    int flag = 1;
    setsockopt(new_socket, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag));

    // Load and send the list of loans
    FILE *fp = fopen("loans.txt", "r");
    if (!fp)
    {
        send(new_socket, "Error opening loans file.\n", 26, 0);
        return;
    }

    char line[512], list_buffer[4096];
    snprintf(list_buffer, sizeof(list_buffer), "Loan Applications assigned to you:\n");
    size_t len = strlen(list_buffer);

    while (fgets(line, sizeof(line), fp))
    {
        char loanID[32], username[64], status[32], empID[16];
        double amount;
        if (sscanf(line, "%[^|]|%[^|]|%lf|%[^|]|%s", loanID, username, &amount, status, empID) == 5)
        {
            if (strcmp(empID, employee_id) == 0)
            {
                if (len + strlen(line) + 2 < sizeof(list_buffer))
                {
                    strcat(list_buffer, line);
                    strcat(list_buffer, "\n");
                    len += strlen(line) + 1;
                }
            }
        }
    }
    fclose(fp);

    if (len == strlen("Loan Applications assigned to you:\n"))
    {
        strcat(list_buffer, "No loan applications found.\n");
    }

    // Send loan list
    send(new_socket, list_buffer, strlen(list_buffer), 0);

    // Send prompt
    const char *prompt = "Enter loan ID to process or 'back' to return:\n";
    send(new_socket, prompt, strlen(prompt), 0);

    // Wait for client's input
    char buffer[128];
    int rlen = read(new_socket, buffer, sizeof(buffer) - 1);
    if (rlen <= 0)
        return;
    buffer[rlen] = '\0';
    buffer[strcspn(buffer, "\n")] = 0; // trim newline

    if (strcmp(buffer, "back") == 0)
    {
        send(new_socket, "Returned to main menu.\n", 23, 0);
        return;
    }

    // Update loan status
    FILE *in_fp = fopen("loans.txt", "r");
    FILE *out_fp = fopen("loans.tmp", "w");
    int found = 0;
    while (fgets(line, sizeof(line), in_fp))
    {
        char loanID[32], username[64], status[32], empID[16];
        double amount;
        if (sscanf(line, "%[^|]|%[^|]|%lf|%[^|]|%s", loanID, username, &amount, status, empID) == 5)
        {
            if (strcmp(loanID, buffer) == 0)
            {
                fprintf(out_fp, "%s|%s|%.2lf|%s|%s\n", loanID, username, amount, "Approved", empID);
                found = 1;
            }
            else
            {
                fputs(line, out_fp);
            }
        }
        else
        {
            fputs(line, out_fp);
        }
    }

    fclose(in_fp);
    fclose(out_fp);

    if (!found)
    {
        send(new_socket, "Loan not found.\n", 15, 0);
        remove("loans.tmp");
        return;
    }

    rename("loans.tmp", "loans.txt");
    send(new_socket, "Loan marked as Approved.\n", 27, 0);
}

// void approve_loan(int new_socket, const char *employee_id) {
//     // Disable Nagle's algorithm to flush packets immediately
//     int flag = 1;
//     setsockopt(new_socket, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag));

//     // Read loans assigned to the employee (regardless of their status)
//     FILE *fp = fopen("loans.txt", "r");
//     if (!fp) {
//         send(new_socket, "Error opening loans file.\n", 26, 0);
//         return;
//     }

//     char line[512], list_buffer[4096];
//     snprintf(list_buffer, sizeof(list_buffer), "Loans assigned to you:\n");
//     size_t len = strlen(list_buffer);

//     while (fgets(line, sizeof(line), fp)) {
//         char loanID[32], username[64], status[32], empID[16];
//         double amount;

//         if (sscanf(line, "%31[^|]|%63[^|]|%lf|%31[^|]|%15s", loanID, username, &amount, status, empID) == 5) {
//             if (strcmp(empID, employee_id) == 0) {
//                 if (len + strlen(line) + 2 < sizeof(list_buffer)) {
//                     strcat(list_buffer, line);
//                     strcat(list_buffer, "\n");
//                     len += strlen(line) + 1;
//                 }
//             }
//         }
//     }
//     fclose(fp);

//     if (len == strlen("Loans assigned to you:\n")) {
//         strcat(list_buffer, "No loans assigned to you.\n");
//     }

//     send(new_socket, list_buffer, strlen(list_buffer), 0);

//     const char *prompt = "Enter loan ID to approve or 'back' to return:\n";
//     send(new_socket, prompt, strlen(prompt), 0);

//     char buffer[128];
//     int rlen = read(new_socket, buffer, sizeof(buffer) - 1);
//     if (rlen <= 0) return;
//     buffer[rlen] = 0;
//     buffer[strcspn(buffer, "\n")] = 0;

//     if (strcmp(buffer, "back") == 0) {
//         send(new_socket, "Returned to employee menu.\n", 27, 0);
//         return;
//     }

//     // Update loan status to Approved
//     FILE *in_fp = fopen("loans.txt", "r");
//     FILE *out_fp = fopen("loans.tmp", "w");
//     if (!in_fp || !out_fp) {
//         if (in_fp) fclose(in_fp);
//         if (out_fp) fclose(out_fp);
//         send(new_socket, "Error processing loans file.\n", 29, 0);
//         return;
//     }

//     int found = 0;
//     while (fgets(line, sizeof(line), in_fp)) {
//         char loanID[32], username[64], status[32], empID[16];
//         double amount;
//         if (sscanf(line, "%31[^|]|%63[^|]|%lf|%31[^|]|%15s", loanID, username, &amount, status, empID) == 5) {
//             if (strcmp(loanID, buffer) == 0 && strcmp(empID, employee_id) == 0) {
//                 fprintf(out_fp, "%s|%s|%.2lf|%s|%s\n", loanID, username, amount, "Approved", empID);
//                 found = 1;
//             } else {
//                 fputs(line, out_fp);
//             }
//         } else {
//             fputs(line, out_fp);
//         }
//     }

//     fclose(in_fp);
//     fclose(out_fp);

//     if (!found) {
//         send(new_socket, "Loan not found or not assigned to you.\n", 38, 0);
//         remove("loans.tmp");
//         return;
//     }

//     if (rename("loans.tmp", "loans.txt") != 0) {
//         send(new_socket, "Error updating loan status.\n", 27, 0);
//         return;
//     }

//     send(new_socket, "Loan approved successfully.\n", 27, 0);
// }

// void reject_loan(int new_socket, const char *employee_id) {
//     FILE *fp = fopen("loans.txt", "r");
//     if (!fp) {
//         send(new_socket, "Error opening loans file.\n", 26, 0);
//         return;
//     }

//     char line[512], list_buffer[4096];
//     snprintf(list_buffer, sizeof(list_buffer), "Loans In-Process assigned to you:\n");
//     size_t len = strlen(list_buffer);

//     while (fgets(line, sizeof(line), fp)) {
//         char loanID[32], username[64], status[32], empID[16];
//         double amount;
//         if (sscanf(line, "%31[^|]|%63[^|]|%lf|%31[^|]|%15s", loanID, username, &amount, status, empID) == 5) {
//             if (strcmp(empID, employee_id) == 0 && strcmp(status, "In-Process") == 0) {
//                 if (len + strlen(line) + 2 < sizeof(list_buffer)) {
//                     strcat(list_buffer, line);
//                     strcat(list_buffer, "\n");
//                     len += strlen(line) + 1;
//                 }
//             }
//         }
//     }
//     fclose(fp);

//     if (len == strlen("Loans In-Process assigned to you:\n")) {
//         strcat(list_buffer, "No loans pending rejection.\n");
//         send(new_socket, list_buffer, strlen(list_buffer), 0);
//         return;
//     }

//     send(new_socket, list_buffer, strlen(list_buffer), 0);
//     send(new_socket, "Enter loan ID to reject or 'back' to return:\n", 45, 0);

//     char buffer[128];
//     int rlen = read(new_socket, buffer, sizeof(buffer) - 1);
//     if (rlen <= 0) return;
//     buffer[rlen] = '\0';
//     buffer[strcspn(buffer, "\n")] = 0;

//     if (strcmp(buffer, "back") == 0) {
//         send(new_socket, "Returned to employee menu.\n", 27, 0);
//         return;
//     }

//     FILE *in_fp = fopen("loans.txt", "r");
//     FILE *out_fp = fopen("loans.tmp", "w");
//     if (!in_fp || !out_fp) {
//         if (in_fp) fclose(in_fp);
//         if (out_fp) fclose(out_fp);
//         send(new_socket, "Error processing loans file.\n", 29, 0);
//         return;
//     }

//     int found = 0;
//     while (fgets(line, sizeof(line), in_fp)) {
//         char loanID[32], username[64], status[32], empID[16];
//         double amount;
//         if (sscanf(line, "%31[^|]|%63[^|]|%lf|%31[^|]|%15s", loanID, username, &amount, status, empID) == 5) {
//             if (strcmp(loanID, buffer) == 0 && strcmp(status, "In-Process") == 0 && strcmp(empID, employee_id) == 0) {
//                 fprintf(out_fp, "%s|%s|%.2lf|%s|%s\n", loanID, username, amount, "Rejected", empID);
//                 found = 1;
//             } else {
//                 fputs(line, out_fp);
//             }
//         } else {
//             fputs(line, out_fp);
//         }
//     }

//     fclose(in_fp);
//     fclose(out_fp);

//     if (!found) {
//         send(new_socket, "Loan not found or not in In-Process status.\n", 44, 0);
//         remove("loans.tmp");
//         return;
//     }

//     if (rename("loans.tmp", "loans.txt") != 0) {
//         send(new_socket, "Error updating loan status.\n", 27, 0);
//         return;
//     }

//     send(new_socket, "Loan rejected successfully.\n", 27, 0);
// }

// void view_customer_transactions(int new_socket) {
//     // TODO: implement properly
// }

// void change_password(int new_socket) {

// }

bool get_assigned_loans(const char *username, char *output_buffer, size_t buf_size)
{
    if (!username || !output_buffer)
        return false;

    // Open employee_data.txt to find employee id
    FILE *fp_emp = fopen("employee_data.txt", "r");
    if (!fp_emp)
        return false;
    int fd_emp = fileno(fp_emp);
    if (lock_file(fd_emp, F_RDLCK) != 0)
    {
        fclose(fp_emp);
        return false;
    }

    char line[256];
    char file_empid[32] = {0}, file_username[64], mobile[32];
    bool found_empid = false;

    while (fgets(line, sizeof(line), fp_emp))
    {
        int parsed = sscanf(line, "%31[^|]|%63[^|]|%31[^\n]", file_empid, file_username, mobile);
        if (parsed == 3)
        {
            trim(file_username);
            if (strcmp(file_username, username) == 0)
            {
                found_empid = true;
                break;
            }
        }
    }

    unlock_file(fd_emp);
    fclose(fp_emp);

    if (!found_empid)
        return false;

    // Open loans.txt to find loans assigned to employeeid
    FILE *fp_loans = fopen("loans.txt", "r");
    if (!fp_loans)
        return false;
    int fd_loans = fileno(fp_loans);
    if (lock_file(fd_loans, F_RDLCK) != 0)
    {
        fclose(fp_loans);
        return false;
    }

    output_buffer[0] = '\0';
    size_t output_len = 0;
    char loan_id[32], account_num[64], status[32], loan_empid[32];
    double amount;

    // Header for output
    static const char *header = "LoanID | AccountNumber | Amount | Status | Assigned Employee\n"
                                "---------------------------------------------------------------\n";

    strncat(output_buffer, header, buf_size - 1);
    output_len = strlen(output_buffer);

    while (fgets(line, sizeof(line), fp_loans))
    {
        int parsed = sscanf(line, "%31[^|]|%63[^|]|%lf|%31[^|]|%31[^\n]",
                            loan_id, account_num, &amount, status, loan_empid);
        if (parsed == 5)
        {
            trim(loan_empid);
            if (strcmp(loan_empid, file_empid) == 0)
            {
                int written = snprintf(output_buffer + output_len, buf_size - output_len,
                                       "%s | %s | %.2lf | %s | %s\n",
                                       loan_id, account_num, amount, status, loan_empid);
                if (written < 0 || (size_t)written >= buf_size - output_len)
                    break; // Buffer full, stop reading more loans

                output_len += written;
            }
        }
    }

    unlock_file(fd_loans);
    fclose(fp_loans);

    return (output_len > strlen(header)); // True if at least one loan added
}

bool get_transaction_history_by_account(const char *accountnumber, char *output_buffer, size_t buf_size)
{
    if (!accountnumber || !output_buffer)
        return false;

    // Open transactions.txt with shared read lock
    FILE *fp_tx = fopen("transactions.txt", "r");
    if (!fp_tx)
        return false;

    int fd_tx = fileno(fp_tx);
    if (lock_file(fd_tx, F_RDLCK) != 0)
    {
        fclose(fp_tx);
        return false;
    }

    output_buffer[0] = '\0';
    char tx_line[512], acc_in_tx[64];

    // Read lines matching accountnumber
    while (fgets(tx_line, sizeof(tx_line), fp_tx))
    {
        if (sscanf(tx_line, "%63[^|]|", acc_in_tx) == 1)
        {
            trim(acc_in_tx);
            if (strcmp(acc_in_tx, accountnumber) == 0)
            {
                if ((strlen(output_buffer) + strlen(tx_line) + 1) < buf_size)
                {
                    strcat(output_buffer, tx_line);
                }
                else
                {
                    break; // buffer full
                }
            }
        }
    }

    unlock_file(fd_tx);
    fclose(fp_tx);

    return (strlen(output_buffer) > 0);
}
