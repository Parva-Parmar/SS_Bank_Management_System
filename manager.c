#include "manager.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include "utils.h"
#define USERS_FILE "users.txt"
#define EMPLOYEE_DATA_FILE "employee_data.txt"
#define CUSTOMER_DATA_FILE "customer_data.txt"
extern int lock_file(int fd, int lock_type);
extern int unlock_file(int fd);

int validate_manager_login(const char *username, const char *password) {
    FILE *fp = fopen(USERS_FILE, "r");
    if (!fp) {
        return 0;  // Cannot open file, treat as login failure
    }

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        char role[32], file_user[64], file_pass[64];
        if (sscanf(line, "%31[^|]|%63[^|]|%63[^\n]", role, file_user, file_pass) == 3) {
            if (strcmp(role, "manager") == 0 &&
                strcmp(file_user, username) == 0 &&
                strcmp(file_pass, password) == 0) {
                fclose(fp);
                return 1;  // Valid manager credentials
            }
        }
    }

    fclose(fp);
    return 0;  // No valid match found
}

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

bool employee_id_exists(const char *employeeID) {
    FILE *fp = fopen("employee_data.txt", "r");
    if (!fp) return false;

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        char curr_id[16];
        sscanf(line, "%15[^|]", curr_id);
        if (strcmp(curr_id, employeeID) == 0) {
            fclose(fp);
            return true;
        }
    }
    fclose(fp);
    return false;
}



bool manager_set_customer_account_status(const char *accountnum, const char *status) {
    if (!accountnum || (!status || (strcmp(status, "Active") && strcmp(status, "Deactive")))) {
        return false; // Invalid input
    }

    int fd = open("customer_data.txt", O_RDWR);
    if (fd < 0) return false;

    if (lock_file(fd, F_WRLCK) < 0) {
        close(fd);
        return false;
    }

    char buffer[8192];
    ssize_t bytes_read = 0;
    ssize_t total_read = 0;
    while ((bytes_read = read(fd, buffer + total_read, sizeof(buffer) - total_read)) > 0) {
        total_read += bytes_read;
        if (total_read >= sizeof(buffer)) break;
    }
    if (bytes_read < 0) {
        unlock_file(fd);
        close(fd);
        return false;
    }

    char *saveptr;
    char *line = strtok_r(buffer, "\n", &saveptr);
    bool modified = false;
    char new_content[8192];
    new_content[0] = '\0';

    while (line) {
        char line_copy[256];
        strncpy(line_copy, line, sizeof(line_copy) - 1);
        line_copy[sizeof(line_copy) - 1] = '\0';

        // Parse line: username|accountnum|mobilenum|balance|status
        char cust_username[64], curr_accountnum[32], mobilenumber[32], balance[32], current_status[16];
        if (sscanf(line_copy, "%63[^|]|%31[^|]|%31[^|]|%31[^|]|%15s",
                   cust_username, curr_accountnum, mobilenumber, balance, current_status) == 5) {
            if (strcmp(curr_accountnum, accountnum) == 0) {
                // Update status field
                snprintf(current_status, sizeof(current_status), "%s", status);
                modified = true;
            }
            char new_line[256];
            snprintf(new_line, sizeof(new_line), "%s|%s|%s|%s|%s\n", cust_username, curr_accountnum, mobilenumber, balance, current_status);
            strncat(new_content, new_line, sizeof(new_content) - strlen(new_content) - 1);
        } else {
            // Keep line as is
            strncat(new_content, line_copy, sizeof(new_content) - strlen(new_content) - 1);
            strncat(new_content, "\n", sizeof(new_content) - strlen(new_content) - 1);
        }
        line = strtok_r(NULL, "\n", &saveptr);
    }

    if (modified) {
        if (ftruncate(fd, 0) < 0) {
            unlock_file(fd);
            close(fd);
            return false;
        }
        lseek(fd, 0, SEEK_SET);
        if (write(fd, new_content, strlen(new_content)) != (ssize_t)strlen(new_content)) {
            unlock_file(fd);
            close(fd);
            return false;
        }
    }

    unlock_file(fd);
    close(fd);
    return modified;
}
int assign_loan_to_employee(const char *loanID, const char *employeeID) {
    if (!loanID || !employeeID) return -1;

    // Check employee ID validity
    if (!employee_id_exists(employeeID)) {
        return 2; // invalid employeeID
    }

    int fd = open("loans.txt", O_RDWR);
    if (fd < 0) {
        return -1; // system/file error
    }

    if (lock_file(fd, F_WRLCK) < 0) {
        close(fd);
        return -1; // locking error
    }

    char buffer[8192];
    ssize_t total_read = 0, bytes_read;

    while ((bytes_read = read(fd, buffer + total_read, sizeof(buffer) - total_read)) > 0) {
        total_read += bytes_read;
        if (total_read >= sizeof(buffer)) break;
    }
    if (bytes_read < 0) {
        unlock_file(fd);
        close(fd);
        return -1;
    }
    buffer[total_read] = '\0';

    typedef struct {
        char loan_id[32];
        char account_num[32];
        char amount[32];
        char status[32];
        char employee_id[32];
    } LoanRecord;

    LoanRecord loans[100];
    int loan_count = 0;
    int target_loan_index = -1;

    char *saveptr;
    char *line = strtok_r(buffer, "\n", &saveptr);

    while (line && loan_count < 100) {
        LoanRecord lr;
        if (sscanf(line, "%31[^|]|%31[^|]|%31[^|]|%31[^|]|%31s",
                   lr.loan_id, lr.account_num, lr.amount, lr.status, lr.employee_id) == 5) {
            loans[loan_count++] = lr;
            if (strcmp(lr.loan_id, loanID) == 0) {
                target_loan_index = loan_count - 1;
            }
        }
        line = strtok_r(NULL, "\n", &saveptr);
    }

    if (target_loan_index == -1) {
        unlock_file(fd);
        close(fd);
        return 1; // invalid loanID
    }

    // Check if loan already assigned
    if (strcmp(loans[target_loan_index].employee_id, "U000000") != 0) {
        unlock_file(fd);
        close(fd);
        return 3; // loan already assigned
    }

    // Assign loan
    strncpy(loans[target_loan_index].employee_id, employeeID, sizeof(loans[target_loan_index].employee_id) - 1);
    loans[target_loan_index].employee_id[sizeof(loans[target_loan_index].employee_id) -1] = '\0';

    // Rebuild loans file content
    char new_content[8192] = "";
    for (int i = 0; i < loan_count; i++) {
        char line[256];
        snprintf(line, sizeof(line), "%s|%s|%s|%s|%s\n",
                 loans[i].loan_id,
                 loans[i].account_num,
                 loans[i].amount,
                 loans[i].status,
                 loans[i].employee_id);
        strncat(new_content, line, sizeof(new_content) - strlen(new_content) - 1);
    }

    // Write updated file content atomically
    if (ftruncate(fd, 0) < 0) {
        unlock_file(fd);
        close(fd);
        return -1;
    }
    lseek(fd, 0, SEEK_SET);
    if (write(fd, new_content, strlen(new_content)) != (ssize_t)strlen(new_content)) {
        unlock_file(fd);
        close(fd);
        return -1;
    }

    unlock_file(fd);
    close(fd);

 

    return 0; // success
}



// Review customer feedback
void manager_review_customer_feedback(void) {
    // To be implemented
}

// Change manager's password
bool change_password(const char *username, const char *new_password) {
    FILE *fp = fopen("users.txt", "r+");
    if (!fp) return false;

    int fd = fileno(fp);
    if (lock_file(fd, F_WRLCK) < 0) {
        fclose(fp);
        return false;
    }

    char line[256];
    long pos = 0;
    bool updated = false;

    while (fgets(line, sizeof(line), fp)) {
        char role[32], user[64], pass[64];
        if (sscanf(line, "%31[^|]|%63[^|]|%63[^\n]", role, user, pass) == 3) {
            trimm(user);
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

    unlock_file(fd);
    fclose(fp);
    return updated;
}

// Logout function placeholder
void manager_logout(void) {
    // To be implemented
}
