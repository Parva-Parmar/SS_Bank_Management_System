#include "admin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <ctype.h>

// Trim leading and trailing whitespace in-place
void trimm(char *str) {
    char *end;
    // Trim leading space
    while (isspace((unsigned char)*str)) str++;

    if (*str == 0)  // All spaces?
        return;

    // Trim trailing space
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;

    // Write new null terminator
    *(end + 1) = 0;
}

static pthread_mutex_t *log_mutex_internal = NULL;

#define USERS_FILE "users.txt"
#define EMPLOYEE_DATA_FILE "employee_data.txt"
#define CUSTOMER_DATA_FILE "customer_data.txt"

// Helper function to lock a whole file using fcntl
static int lock_file(int fd, int lock_type) {
    struct flock lock;
    memset(&lock, 0, sizeof(lock));
    lock.l_type = lock_type;  // F_WRLCK or F_RDLCK
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;  // 0 means lock the entire file
    return fcntl(fd, F_SETLKW, &lock);
}

// Helper to unlock file
static int unlock_file(int fd) {
    struct flock lock;
    memset(&lock, 0, sizeof(lock));
    lock.l_type = F_UNLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    return fcntl(fd, F_SETLK, &lock);
}

void admin_init(pthread_mutex_t *log_mutex) {
    log_mutex_internal = log_mutex;
}
int validate_admin_login(const char *username, const char *password) {
    FILE *fp = fopen(USERS_FILE, "r");
    if (!fp) {
        return 0;  // Cannot open file, treat as login failure
    }

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        char role[32], file_user[64], file_pass[64];
        if (sscanf(line, "%31[^|]|%63[^|]|%63[^\n]", role, file_user, file_pass) == 3) {
            if (strcmp(role, "admin") == 0 &&
                strcmp(file_user, username) == 0 &&
                strcmp(file_pass, password) == 0) {
                fclose(fp);
                return 1;  // Valid admin credentials
            }
        }
    }

    fclose(fp);
    return 0;  // No valid match found
}


// Get the next unique employee id number based on role ('E' or 'M')
static int get_next_employee_id() {
    int fd = open(EMPLOYEE_DATA_FILE, O_RDONLY);
    if (fd < 0) {
        // File doesn't exist, start IDs from 1
        return 1;
    }
    lock_file(fd, F_RDLCK);

    char buffer[256];
    int max_id = 0;

    FILE *fp = fdopen(fd, "r");
    if (!fp) {
        close(fd);
        return 1;
    }

    while (fgets(buffer, sizeof(buffer), fp)) {
        char id[16], username[64], mobile[32];
        if (sscanf(buffer, "%15[^|]|%63[^|]|%31[^\n]", id, username, mobile) == 3) {
            // Extract numeric part by skipping first character
            int num = atoi(id + 1);
            if (num > max_id) {
                max_id = num;
            }
        }
    }

    fclose(fp); // also closes fd and unlocks

    return max_id + 1;
}


int add_new_employee(const char *role, const char *username, const char *password, const char *mobile) {
    char buffer[256];

    // Open the users file for reading and writing to check for duplicates and append
    int fd_users = open(USERS_FILE, O_RDWR | O_CREAT, 0644);
    if (fd_users < 0) {
        return -1;
    }

    if (lock_file(fd_users, F_WRLCK) < 0) {
        close(fd_users);
        return -1;
    }

    FILE *fp_users = fdopen(fd_users, "r+");
    if (!fp_users) {
        unlock_file(fd_users);
        close(fd_users);
        return -1;
    }

    // Check for duplicate username
    while (fgets(buffer, sizeof(buffer), fp_users)) {
        char stored_role[32], stored_user[64], stored_pass[64];
        sscanf(buffer, "%31[^|]|%63[^|]|%63[^\n]", stored_role, stored_user, stored_pass);
        if (strcmp(stored_user, username) == 0) {
            fclose(fp_users); // fclose closes fd_users and unlocks due to fdopen
            return -2;  // Username exists
        }
    }

    // Generate new employee id
    char id_prefix = (role[0] == 'M' || role[0] == 'm') ? 'M' : 'E';
    int new_id_num = get_next_employee_id((const char[]){id_prefix, '\0'});
    char employee_id[16];
    snprintf(employee_id, sizeof(employee_id), "%c%06d", id_prefix, new_id_num);

    // Append new user record to USERS_FILE
    fprintf(fp_users, "%s|%s|%s\n", role, username, password);
    fflush(fp_users); // flush before unlocking

    fclose(fp_users); // unlocks and closes fd_users

    // Append employee details to employee_data.txt using system calls and locking
    int fd_emp = open(EMPLOYEE_DATA_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd_emp < 0) {
        return -1;
    }

    if (lock_file(fd_emp, F_WRLCK) < 0) {
        close(fd_emp);
        return -1;
    }

    char emp_line[256];
    int len = snprintf(emp_line, sizeof(emp_line), "%s|%s|%s\n", employee_id, username, mobile);
    ssize_t written = write(fd_emp, emp_line, len);

    unlock_file(fd_emp);
    close(fd_emp);

    if (written != len) {
        return -1;
    }

    return 0; // success
}

bool update_customer_mobile(const char *username, const char *new_mobile) {
    int fd = open(CUSTOMER_DATA_FILE, O_RDWR);
    if (fd < 0) return false;

    if (lock_file(fd, F_WRLCK) < 0) {
        close(fd);
        return false;
    }

    FILE *fp = fdopen(fd, "r+");
    if (!fp) {
        unlock_file(fd);
        close(fd);
        return false;
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
            if (strcmp(customers[count].username, username) == 0) {
                strncpy(customers[count].mobile, new_mobile, sizeof(customers[count].mobile) - 1);
                customers[count].mobile[sizeof(customers[count].mobile) - 1] = '\0';
                found = true;
            }
            count++;
        }
    }

    if (!found) {
        unlock_file(fd);
        fclose(fp);
        return false;
    }

    rewind(fp);
    if (ftruncate(fd, 0) < 0) {
        unlock_file(fd);
        fclose(fp);
        return false;
    }

    for (size_t i = 0; i < count; i++) {
        fprintf(fp, "%s|%s|%s|%.2f|%s\n",
                customers[i].username,
                customers[i].account_number,
                customers[i].mobile,
                customers[i].balance,
                customers[i].loan_status);
    }

    fflush(fp);
    unlock_file(fd);
    fclose(fp);

    return true;
}

bool update_employee_mobile(const char *username, const char *new_mobile) {
    int fd = open(EMPLOYEE_DATA_FILE, O_RDWR);
    if (fd < 0) return false;

    if (lock_file(fd, F_WRLCK) < 0) {
        close(fd);
        return false;
    }

    FILE *fp = fdopen(fd, "r+");
    if (!fp) {
        unlock_file(fd);
        close(fd);
        return false;
    }

    typedef struct {
        char id[16];
        char username[64];
        char mobile[16];
    } employee_t;

    employee_t employees[1000];
    size_t count = 0;
    bool found = false;
    char line[256];

    while (fgets(line, sizeof(line), fp) && count < 1000) {
        if (sscanf(line, "%15[^|]|%63[^|]|%15[^\n]",
                   employees[count].id,
                   employees[count].username,
                   employees[count].mobile) == 3) {
            if (strcmp(employees[count].username, username) == 0) {
                strncpy(employees[count].mobile, new_mobile, sizeof(employees[count].mobile) - 1);
                employees[count].mobile[sizeof(employees[count].mobile) - 1] = '\0';
                found = true;
            }
            count++;
        }
    }

    if (!found) {
        unlock_file(fd);
        fclose(fp);
        return false;
    }

    rewind(fp);
    if (ftruncate(fd, 0) < 0) {
        unlock_file(fd);
        fclose(fp);
        return false;
    }

    for (size_t i = 0; i < count; i++) {
        fprintf(fp, "%s|%s|%s\n",
                employees[i].id,
                employees[i].username,
                employees[i].mobile);
    }

    fflush(fp);
    unlock_file(fd);
    fclose(fp);

    return true;
}


int admin_modify_user(const char *username, const char *field, const char *new_value) {
    if (strcmp(field, "password") != 0) {
        // Only 'password' field supported currently
        return -2;
    }
    
    int fd = open(USERS_FILE, O_RDWR);
    if (fd < 0) {
        return -1;  // Cannot open file
    }
    
    if (lock_file(fd, F_WRLCK) < 0) {
        close(fd);
        return -1;
    }
    
    FILE *fp = fdopen(fd, "r+");
    if (!fp) {
        unlock_file(fd);
        close(fd);
        return -1;
    }
    
    char lines[1000][1024];
    int count = 0;
    bool found = false;
    
    // Read all lines
    while (fgets(lines[count], sizeof(lines[count]), fp) && count < 1000) {
        count++;
    }
    
    // Modify only the matching user's password
    for (int i = 0; i < count; i++) {
        char role[32], uname[64], pass[64];
        if (sscanf(lines[i], "%31[^|]|%63[^|]|%63[^\n]", role, uname, pass) == 3) {
            if (strcmp(uname, username) == 0) {
                snprintf(lines[i], sizeof(lines[i]), "%s|%s|%s\n", role, uname, new_value);
                found = true;
                break;
            }
        }
    }
    
    if (!found) {
        fclose(fp);
        // fclose closes fd and unlocks
        return -3;  // User not found
    }
    
    // Truncate the file and write modified data
    rewind(fp);
    if (ftruncate(fd, 0) != 0) {
        fclose(fp);
        return -1;  // Error truncating file
    }
    
    for (int i = 0; i < count; i++) {
        fputs(lines[i], fp);
    }
    fflush(fp);
    fclose(fp);
    
    return 0;  // Success
}


int modify_user_details(const char *user_type, const char *username, const char *new_password, const char *new_mobile) {
    int res;

    // Update password if new_password is not empty
    if (new_password && new_password[0] != '\0') {
        res = admin_modify_user(username, "password", new_password);
        if (res != 0) {
            return -1; // Password update failed
        }
    }

    // Update mobile if new_mobile is not empty
    if (new_mobile && new_mobile[0] != '\0') {
        bool mobile_updated = false;
        if (strcasecmp(user_type, "customer") == 0) {
            mobile_updated = update_customer_mobile(username, new_mobile);
        } else if (strcasecmp(user_type, "employee") == 0 || strcasecmp(user_type, "manager") == 0) {
            mobile_updated = update_employee_mobile(username, new_mobile);
        } else {
            // Unknown user type
            return -2;
        }

        if (!mobile_updated) {
            return -3; // Mobile update failed
        }
    }
    return 0; // Success
}


bool is_employee_in_loans(const char *employee_id) {
    FILE *fp = fopen("loans.txt", "r");
    if (!fp) return false;

    char line[256];
    char trimmed_emp_id[32];

    // copy input id and trim whitespace/newline
    strncpy(trimmed_emp_id, employee_id, sizeof(trimmed_emp_id) - 1);
    trimmed_emp_id[sizeof(trimmed_emp_id) - 1] = '\0';
    trimm(trimmed_emp_id);

    while (fgets(line, sizeof(line), fp)) {
        char loadID[16], custuname[64], amount_str[32], status[32], emp_id[16];
        if (sscanf(line, "%15[^|]|%63[^|]|%31[^|]|%31[^|]|%15[^\n]", loadID, custuname, amount_str, status, emp_id) == 5) {
            trimm(emp_id); // trim trailing newline/space
            if (strcmp(emp_id, trimmed_emp_id) == 0) {
                fclose(fp);
                return true;
            }
        }
    }
    fclose(fp);
    return false;
}

int change_user_role(const char *username) {
    int fd_emp = open(EMPLOYEE_DATA_FILE, O_RDWR);
    if (fd_emp < 0) return -1;

    if (lock_file(fd_emp, F_WRLCK) < 0) {
        close(fd_emp);
        return -1;
    }

    FILE *fp_emp = fdopen(fd_emp, "r+");
    if (!fp_emp) {
        unlock_file(fd_emp);
        close(fd_emp);
        return -1;
    }

    typedef struct {
        char id[16];
        char username[64];
        char mobile[16];
    } employee_t;

    employee_t employees[1000];
    size_t count = 0;
    bool found = false;
    int found_index = -1;
    char old_role_prefix = 0;
    char line[256];

    while (fgets(line, sizeof(line), fp_emp) && count < 1000) {
        if (sscanf(line, "%15[^|]|%63[^|]|%15[^\n]", employees[count].id, employees[count].username, employees[count].mobile) == 3) {
            if (strcmp(employees[count].username, username) == 0) {
                found = true;
                found_index = count;
                old_role_prefix = employees[count].id[0];
            }
            count++;
        }
    }

    if (!found) {
        fclose(fp_emp);
        return -2;  // User not found
    }

    // Check if employee has active loans using the actual employee id found
    if (is_employee_in_loans(employees[found_index].id)) {
        fclose(fp_emp);
        return -3;  // Cannot change roles due to active loans
    }

    // Determine new role prefix, keep numeric part unchanged
    char new_id[16];
    if (old_role_prefix == 'E' || old_role_prefix == 'e') {
        new_id[0] = 'M';
    } else if (old_role_prefix == 'M' || old_role_prefix == 'm') {
        new_id[0] = 'E';
    } else {
        fclose(fp_emp);
        return -4; // Invalid role prefix
    }
    strcpy(new_id + 1, employees[found_index].id + 1);
    new_id[sizeof(new_id) - 1] = '\0';

    // Update employee ID with new prefix
    strncpy(employees[found_index].id, new_id, sizeof(employees[found_index].id) - 1);
    employees[found_index].id[sizeof(employees[found_index].id) - 1] = '\0';

    // Rewrite employee_data.txt fully
    rewind(fp_emp);
    if (ftruncate(fd_emp, 0) < 0) {
        fclose(fp_emp);
        return -1;
    }
    for (size_t i = 0; i < count; ++i) {
        fprintf(fp_emp, "%s|%s|%s\n", employees[i].id, employees[i].username, employees[i].mobile);
    }
    fflush(fp_emp);
    fclose(fp_emp);

    // Update role field in users.txt accordingly
    int fd_users = open(USERS_FILE, O_RDWR);
    if (fd_users < 0) return -1;

    if (lock_file(fd_users, F_WRLCK) < 0) {
        close(fd_users);
        return -1;
    }

    FILE *fp_users = fdopen(fd_users, "r+");
    if (!fp_users) {
        unlock_file(fd_users);
        close(fd_users);
        return -1;
    }

    char user_lines[1000][1024];
    int user_count = 0;
    bool user_found = false;
    while (fgets(user_lines[user_count], sizeof(user_lines[user_count]), fp_users) && user_count < 1000) {
        char role[32], uname[64], pass[64];
        if (sscanf(user_lines[user_count], "%31[^|]|%63[^|]|%63[^\n]", role, uname, pass) == 3) {
            if (strcmp(uname, username) == 0) {
                user_found = true;
                if (strcasecmp(role, "employee") == 0) {
                    snprintf(user_lines[user_count], sizeof(user_lines[user_count]), "manager|%s|%s\n", uname, pass);
                } else if (strcasecmp(role, "manager") == 0) {
                    snprintf(user_lines[user_count], sizeof(user_lines[user_count]), "employee|%s|%s\n", uname, pass);
                }
            }
        }
        user_count++;
    }

    if (!user_found) {
        fclose(fp_users);
        return -2;  // User not found in users.txt
    }

    rewind(fp_users);
    if (ftruncate(fd_users, 0) < 0) {
        fclose(fp_users);
        return -1;
    }

    for (int i = 0; i < user_count; i++) {
        fputs(user_lines[i], fp_users);
    }
    fflush(fp_users);
    fclose(fp_users);

    return 0;  // Role changed successfully
}

bool admin_change_password(const char *username, const char *new_password) {
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
