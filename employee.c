#include "employee.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/types.h>   // For off_t and other types
#include <sys/file.h>    // For flock
#include <sys/socket.h>  // For send, read and sockets
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>


#define USERS_FILE "users.txt"
#define CUSTOMER_DATA_FILE "customer_data.txt"
void trim(char *str);

int validate_employee_login(const char *username, const char *password) {
    FILE *fp = fopen(USERS_FILE, "r");
    if (!fp) {
        return 0;  // Cannot open file, treat as login failure
    }

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        char role[32], file_user[64], file_pass[64];
        if (sscanf(line, "%31[^|]|%63[^|]|%63[^\n]", role, file_user, file_pass) == 3) {
            if (strcmp(role, "employee") == 0 &&
                strcmp(file_user, username) == 0 &&
                strcmp(file_pass, password) == 0) {
                fclose(fp);
                return 1;  // Valid employee credentials
            }
        }
    }
    fclose(fp);
    return 0;  // No valid match found
}
int add_new_customer(const char *username, const char *password, const char *account_number, const char *mobile) {
    char buffer[256];

    // Check if username exists in USERS_FILE
    FILE *fp = fopen(USERS_FILE, "r");
    if (!fp) return -1;
    while (fgets(buffer, sizeof(buffer), fp)) {
        char role[32], file_user[64], file_pass[64];
        sscanf(buffer, "%31[^|]|%63[^|]|%63[^\n]", role, file_user, file_pass);
        if (strcmp(file_user, username) == 0) {
            fclose(fp);
            return -2;  // Username exists
        }
    }
    fclose(fp);

    // Append to USERS_FILE
    fp = fopen(USERS_FILE, "a");
    if (!fp) return -1;
    fprintf(fp, "customer|%s|%s\n", username, password);
    fclose(fp);

    // Append full detailed customer data
    fp = fopen(CUSTOMER_DATA_FILE, "a");
    if (!fp) return -1;
    fprintf(fp, "%s|%s|%s|%.2f|%s\n", username, account_number, mobile, 0.00, "NO_LOAN");
    fclose(fp);

    return 0;
}

void modify_customer_details(int new_socket) {
    char buffer[1024];
    int valread = 0;
    char input[128];
    char account_number_to_modify[16];

    // 1. Ask for account number to modify
    send(new_socket, "Enter account number of customer to modify:\n", 43, 0);
    valread = read(new_socket, account_number_to_modify, sizeof(account_number_to_modify) - 1);
    if (valread <= 0) return;
    account_number_to_modify[valread] = '\0';
    trim(account_number_to_modify);

    // 2. Read all customers into memory from customer_data.txt
    FILE *fp = fopen(CUSTOMER_DATA_FILE, "r");
    if (!fp) {
        send(new_socket, "Error opening customer data file.\n", 34, 0);
        return;
    }

    typedef struct {
        char username[64];
        char account_number[16];
        char mobile[16];
        float balance;
        char loan_status[32];
    } customer_t;

    customer_t customers[1000];
    size_t count = 0;
    bool found = false;
    char line[256];

    while (fgets(line, sizeof(line), fp) && count < 1000) {
        if (sscanf(line, "%63[^|]|%15[^|]|%15[^|]|%f|%31s",
                   customers[count].username,
                   customers[count].account_number,
                   customers[count].mobile,
                   &customers[count].balance,
                   customers[count].loan_status) == 5) {
            if (strcmp(customers[count].account_number, account_number_to_modify) == 0) {
                found = true;
            }
            count++;
        }
    }
    fclose(fp);

    if (!found) {
        send(new_socket, "Account number not found.\n", 26, 0);
        return;
    }

    // 3. Ask if mobile number should be updated
    send(new_socket, "Update mobile number? (yes/no):\n", 33, 0);
    valread = read(new_socket, buffer, sizeof(buffer) - 1);
    if (valread <= 0) return;
    buffer[valread] = '\0';
    trim(buffer);

    if (strcasecmp(buffer, "yes") == 0 || strcasecmp(buffer, "y") == 0) {
        send(new_socket, "Enter new mobile number:\n", 24, 0);
        valread = read(new_socket, buffer, sizeof(buffer) - 1);
        if (valread <= 0) return;
        buffer[valread] = '\0';
        trim(buffer);

        // Update mobile in memory
        for (size_t i = 0; i < count; ++i) {
            if (strcmp(customers[i].account_number, account_number_to_modify) == 0) {
                strncpy(customers[i].mobile, buffer, sizeof(customers[i].mobile) - 1);
                customers[i].mobile[sizeof(customers[i].mobile)-1] = '\0';
                break;
            }
        }
    }

    // 4. Ask if password should be updated
    send(new_socket, "Update password? (yes/no):\n", 28, 0);
    valread = read(new_socket, buffer, sizeof(buffer) - 1);
    if (valread <= 0) return;
    buffer[valread] = '\0';
    trim(buffer);

    if (strcasecmp(buffer, "yes") == 0 || strcasecmp(buffer, "y") == 0) {
        char username[64];

        // Find username by account number
        for (size_t i = 0; i < count; ++i) {
            if (strcmp(customers[i].account_number, account_number_to_modify) == 0) {
                strncpy(username, customers[i].username, sizeof(username) - 1);
                username[sizeof(username)-1] = '\0';
                break;
            }
        }

        send(new_socket, "Enter new password:\n", 20, 0);
        valread = read(new_socket, buffer, sizeof(buffer) - 1);
        if (valread <= 0) return;
        buffer[valread] = '\0';
        trim(buffer);

        // Update password in users.txt file
        // Read all users
        FILE *ufp = fopen(USERS_FILE, "r");
        if (!ufp) {
            send(new_socket, "Error opening users file.\n", 26, 0);
            return;
        }

        char userlines[1000][1024];
        int usercount = 0;
        bool user_found = false;
        while (fgets(userlines[usercount], sizeof(userlines[usercount]), ufp) && usercount < 1000) {
            char role[32], uname[64], upass[64];
            if (sscanf(userlines[usercount], "%31[^|]|%63[^|]|%63[^\n]", role, uname, upass) == 3) {
                if (strcmp(uname, username) == 0) {
                    char safe_password[200];
                    strncpy(safe_password, buffer, sizeof(safe_password) - 1);
                    safe_password[sizeof(safe_password) - 1] = '\0';
                    snprintf(userlines[usercount], sizeof(userlines[usercount]), "customer|%s|%s\n", username, safe_password);
                    user_found = true;
                }
            }
            usercount++;
        }
        fclose(ufp);

        if (!user_found) {
            send(new_socket, "User not found in users file.\n", 30, 0);
            return;
        }

        // Write back updated users file
        ufp = fopen(USERS_FILE, "w");
        if (!ufp) {
            send(new_socket, "Failed to open users file for writing.\n", 39, 0);
            return;
        }
        for (int i = 0; i < usercount; ++i) {
            fputs(userlines[i], ufp);
        }
        fclose(ufp);
    }

    // 5. Rewrite the entire customer_data.txt with updated mobile
    fp = fopen(CUSTOMER_DATA_FILE, "w");
    if (!fp) {
        send(new_socket, "Error opening customer data file for writing.\n", 45, 0);
        return;
    }
    for (size_t i = 0; i < count; ++i) {
        fprintf(fp, "%s|%s|%s|%.2f|%s\n",
                customers[i].username,
                customers[i].account_number,
                customers[i].mobile,
                customers[i].balance,
                customers[i].loan_status);
    }
    fclose(fp);

    send(new_socket, "Customer details updated successfully.\n", 39, 0);
}

void process_loan_applications1(int new_socket, const char *employee_id) {
    // Disable Nagle's algorithm for immediate send
    int flag = 1;
    setsockopt(new_socket, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag));

    // Load and send the list of loans
    FILE *fp = fopen("loans.txt", "r");
    if (!fp) {
        send(new_socket, "Error opening loans file.\n", 26, 0);
        return;
    }

    char line[512], list_buffer[4096];
    snprintf(list_buffer, sizeof(list_buffer), "Loan Applications assigned to you:\n");
    size_t len = strlen(list_buffer);

    while (fgets(line, sizeof(line), fp)) {
        char loanID[32], username[64], status[32], empID[16];
        double amount;
        if (sscanf(line, "%[^|]|%[^|]|%lf|%[^|]|%s", loanID, username, &amount, status, empID) == 5) {
            if (strcmp(empID, employee_id) == 0) {
                if (len + strlen(line) + 2 < sizeof(list_buffer)) {
                    strcat(list_buffer, line);
                    strcat(list_buffer, "\n");
                    len += strlen(line) + 1;
                }
            }
        }
    }
    fclose(fp);

    if (len == strlen("Loan Applications assigned to you:\n")) {
        strcat(list_buffer, "No loan applications found.\n");
    }

    // Send loan list
    send(new_socket, list_buffer, strlen(list_buffer), 0);

    // Send prompt
    const char *prompt = "Enter loan ID to process or 'back' to return:\n";
    send(new_socket, prompt, strlen(prompt), 0);

    // Wait for client's input
    char buffer[128];
    int rlen = read(new_socket, buffer, sizeof(buffer)-1);
    if (rlen <= 0) return;
    buffer[rlen] = '\0';
    buffer[strcspn(buffer, "\n")] = 0; // trim newline

    if (strcmp(buffer, "back") == 0) {
        send(new_socket, "Returned to main menu.\n", 23, 0);
        return;
    }

    // Update loan status
    FILE *in_fp = fopen("loans.txt", "r");
    FILE *out_fp = fopen("loans.tmp", "w");
    int found = 0;
    while (fgets(line, sizeof(line), in_fp)) {
        char loanID[32], username[64], status[32], empID[16];
        double amount;
        if (sscanf(line, "%[^|]|%[^|]|%lf|%[^|]|%s", loanID, username, &amount, status, empID) == 5) {
            if (strcmp(loanID, buffer) == 0) {
                fprintf(out_fp, "%s|%s|%.2lf|%s|%s\n", loanID, username, amount, "Approved", empID);
                found = 1;
            } else {
                fputs(line, out_fp);
            }
        } else {
            fputs(line, out_fp);
        }
    }

    fclose(in_fp);
    fclose(out_fp);

    if (!found) {
        send(new_socket, "Loan not found.\n", 15, 0);
        remove("loans.tmp");
        return;
    }

    rename("loans.tmp", "loans.txt");
    send(new_socket, "Loan marked for processing.\n", 27, 0);
}

void process_loan_applications2(int new_socket, const char *employee_id) {
    // Disable Nagle's algorithm for immediate send
    int flag = 1;
    setsockopt(new_socket, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag));

    // Load and send the list of loans
    FILE *fp = fopen("loans.txt", "r");
    if (!fp) {
        send(new_socket, "Error opening loans file.\n", 26, 0);
        return;
    }

    char line[512], list_buffer[4096];
    snprintf(list_buffer, sizeof(list_buffer), "Loan Applications assigned to you:\n");
    size_t len = strlen(list_buffer);

    while (fgets(line, sizeof(line), fp)) {
        char loanID[32], username[64], status[32], empID[16];
        double amount;
        if (sscanf(line, "%[^|]|%[^|]|%lf|%[^|]|%s", loanID, username, &amount, status, empID) == 5) {
            if (strcmp(empID, employee_id) == 0) {
                if (len + strlen(line) + 2 < sizeof(list_buffer)) {
                    strcat(list_buffer, line);
                    strcat(list_buffer, "\n");
                    len += strlen(line) + 1;
                }
            }
        }
    }
    fclose(fp);

    if (len == strlen("Loan Applications assigned to you:\n")) {
        strcat(list_buffer, "No loan applications found.\n");
    }

    // Send loan list
    send(new_socket, list_buffer, strlen(list_buffer), 0);

    // Send prompt
    const char *prompt = "Enter loan ID to process or 'back' to return:\n";
    send(new_socket, prompt, strlen(prompt), 0);

    // Wait for client's input
    char buffer[128];
    int rlen = read(new_socket, buffer, sizeof(buffer)-1);
    if (rlen <= 0) return;
    buffer[rlen] = '\0';
    buffer[strcspn(buffer, "\n")] = 0; // trim newline

    if (strcmp(buffer, "back") == 0) {
        send(new_socket, "Returned to main menu.\n", 23, 0);
        return;
    }

    // Update loan status
    FILE *in_fp = fopen("loans.txt", "r");
    FILE *out_fp = fopen("loans.tmp", "w");
    int found = 0;
    while (fgets(line, sizeof(line), in_fp)) {
        char loanID[32], username[64], status[32], empID[16];
        double amount;
        if (sscanf(line, "%[^|]|%[^|]|%lf|%[^|]|%s", loanID, username, &amount, status, empID) == 5) {
            if (strcmp(loanID, buffer) == 0) {
                fprintf(out_fp, "%s|%s|%.2lf|%s|%s\n", loanID, username, amount, "Rejected", empID);
                found = 1;
            } else {
                fputs(line, out_fp);
            }
        } else {
            fputs(line, out_fp);
        }
    }

    fclose(in_fp);
    fclose(out_fp);

    if (!found) {
        send(new_socket, "Loan not found.\n", 15, 0);
        remove("loans.tmp");
        return;
    }

    rename("loans.tmp", "loans.txt");
    send(new_socket, "Loan marked for processing.\n", 27, 0);
}
 

// approve 
void approve_loan(int new_socket, const char *employee_id) {
    // Disable Nagle's algorithm for immediate send
    int flag = 1;
    setsockopt(new_socket, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag));

    // Load and send the list of loans
    FILE *fp = fopen("loans.txt", "r");
    if (!fp) {
        send(new_socket, "Error opening loans file.\n", 26, 0);
        return;
    }

    char line[512], list_buffer[4096];
    snprintf(list_buffer, sizeof(list_buffer), "Loan Applications assigned to you:\n");
    size_t len = strlen(list_buffer);

    while (fgets(line, sizeof(line), fp)) {
        char loanID[32], username[64], status[32], empID[16];
        double amount;
        if (sscanf(line, "%[^|]|%[^|]|%lf|%[^|]|%s", loanID, username, &amount, status, empID) == 5) {
            if (strcmp(empID, employee_id) == 0) {
                if (len + strlen(line) + 2 < sizeof(list_buffer)) {
                    strcat(list_buffer, line);
                    strcat(list_buffer, "\n");
                    len += strlen(line) + 1;
                }
            }
        }
    }
    fclose(fp);

    if (len == strlen("Loan Applications assigned to you:\n")) {
        strcat(list_buffer, "No loan applications found.\n");
    }

    // Send loan list
    send(new_socket, list_buffer, strlen(list_buffer), 0);

    // Send prompt
    const char *prompt = "Enter loan ID to process or 'back' to return:\n";
    send(new_socket, prompt, strlen(prompt), 0);

    // Wait for client's input
    char buffer[128];
    int rlen = read(new_socket, buffer, sizeof(buffer)-1);
    if (rlen <= 0) return;
    buffer[rlen] = '\0';
    buffer[strcspn(buffer, "\n")] = 0; // trim newline

    if (strcmp(buffer, "back") == 0) {
        send(new_socket, "Returned to main menu.\n", 23, 0);
        return;
    }

    // Update loan status
    FILE *in_fp = fopen("loans.txt", "r");
    FILE *out_fp = fopen("loans.tmp", "w");
    int found = 0;
    while (fgets(line, sizeof(line), in_fp)) {
        char loanID[32], username[64], status[32], empID[16];
        double amount;
        if (sscanf(line, "%[^|]|%[^|]|%lf|%[^|]|%s", loanID, username, &amount, status, empID) == 5) {
            if (strcmp(loanID, buffer) == 0) {
                fprintf(out_fp, "%s|%s|%.2lf|%s|%s\n", loanID, username, amount, "Approved", empID);
                found = 1;
            } else {
                fputs(line, out_fp);
            }
        } else {
            fputs(line, out_fp);
        }
    }

    fclose(in_fp);
    fclose(out_fp);

    if (!found) {
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
