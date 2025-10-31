#include "customer.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>
#include <time.h>

#define USERS_FILE "users.txt"
#define CUSTOMER_DATA_FILE "customer_data.txt"
#define TRANSACTIONS_FILE "transactions.txt"

static void trim(char *str) {
    char *start = str;
    while (isspace((unsigned char)*start))
        start++;
    char *end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end))
        end--;
    *(end + 1) = '\0';
    memmove(str, start, end - start + 2);
}

static FILE *open_file_locked(const char *filename, const char *mode, int *fd_out) {
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

static void close_file_locked(FILE *fp, int fd) {
    flock(fd, LOCK_UN);
    fclose(fp);
}

void log_transaction_from_account(const char *account, const char *type, float amount, const char *to_account) {
    // Implement file logging here
}

void log_transaction_to_account(const char *account, const char *type, float amount, const char *from_account) {
    // Implement file logging here
}


void log_transaction(const char *username, const char *type, float amount, const char *remarks) {
    int fd;
    FILE *fp = open_file_locked(TRANSACTIONS_FILE, "a", &fd);
    if (!fp)
        return;
    time_t now = time(NULL);
    char *dt = ctime(&now);
    dt[strlen(dt) - 1] = '\0';
    fprintf(fp, "%s|%s|%.2f|%s|%s\n", username, type, amount, dt, remarks);
    close_file_locked(fp, fd);
}

bool validate_customer_login(const char *username, const char *password) {
    FILE *fp = fopen(USERS_FILE, "r");
    if (!fp)
        return false;

    char line[256];
    bool valid = false;
    while (fgets(line, sizeof(line), fp)) {
        char role[32], file_user[64], file_pass[64];
        sscanf(line, "%31[^|]|%63[^|]|%63[^\n]", role, file_user, file_pass);
        trim(role);
        trim(file_user);
        trim(file_pass);
        if (strcmp(role, "customer") == 0 &&
            strcmp(username, file_user) == 0 &&
            strcmp(password, file_pass) == 0) {
            valid = true;
            break;
        }
    }
    fclose(fp);
    return valid;
}

bool customer_exists(const char *username) {
    FILE *fp = fopen(USERS_FILE, "r");
    if (!fp)
        return false;
    char line[256];
    bool found = false;
    while (fgets(line, sizeof(line), fp)) {
        char role[32], user[64], pass[64];
        sscanf(line, "%31[^|]|%63[^|]|%63[^\n]", role, user, pass);
        trim(role);
        trim(user);
        if (strcmp(role, "customer") == 0 && strcmp(user, username) == 0) {
            found = true;
            break;
        }
    }
    fclose(fp);
    return found;
}

bool add_customer(const char *username, const char *password) {
    if (customer_exists(username))
        return false;

    FILE *fp_users = fopen(USERS_FILE, "a");
    if (!fp_users)
        return false;
    fprintf(fp_users, "customer|%s|%s\n", username, password);
    fclose(fp_users);

    FILE *fp_data = fopen(CUSTOMER_DATA_FILE, "a");
    if (!fp_data)
        return false;
    fprintf(fp_data, "%s|0.00|NO_LOAN\n", username);
    fclose(fp_data);

    return true;
}

float get_account_balance(const char *username) {
    FILE *fp = fopen(CUSTOMER_DATA_FILE, "r");
    if (!fp)
        return -1.0f;

    char line[256];
    float balance = -1.0f;

    while (fgets(line, sizeof(line), fp)) {
        char user[64], accountnumber[32], mobile[32], status[32];
        float bal;
        // Parse according to your file format
        sscanf(line, "%63[^|]|%31[^|]|%31[^|]|%f|%31[^|\n]", user, accountnumber, mobile, &bal, status);
        trim(user);
        if (strcmp(username, user) == 0) {
            balance = bal;
            break;
        }
    }

    fclose(fp);
    return balance;
}

bool deposit_money(const char *username, float amount) {
    if (amount <= 0) {
        return false;
    }
    int fd;
    FILE *fp = open_file_locked(CUSTOMER_DATA_FILE, "r+", &fd);
    if (!fp) {
        return false;
    }
    char line[256];
    long pos = 0;
    bool updated = false;
    while (fgets(line, sizeof(line), fp)) {
        char user[64], accountnumber[32], mobilenumber[32], status[32];
        float balance;
        sscanf(line, "%63[^|]|%31[^|]|%31[^|]|%f|%31[^\n]", user, accountnumber, mobilenumber, &balance, status);
        trim(user);
        if (strcmp(user, username) == 0) {
            balance += amount;
            fseek(fp, pos, SEEK_SET);
            fprintf(fp, "%s|%s|%s|%.2f|%s\n", user, accountnumber, mobilenumber, balance, status);
            fflush(fp);
            updated = true;
            close_file_locked(fp, fd);
            log_transaction(username, "Deposit", amount, "Deposit to account");
            return true;
        }
        pos = ftell(fp);
    }
    close_file_locked(fp, fd);
    return updated;
}


bool withdraw_money(const char *username, float amount) {
    if (amount <= 0) return false;

    int fd;
    FILE *fp = open_file_locked(CUSTOMER_DATA_FILE, "r+", &fd);
    if (!fp) return false;

    char line[256];
    long pos = 0;
    bool updated = false;

    while (fgets(line, sizeof(line), fp)) {
        char user[64], accountnumber[32], mobilenumber[32], status[32];
        float balance;
        sscanf(line, "%63[^|]|%31[^|]|%31[^|]|%f|%31[^\n]", user, accountnumber, mobilenumber, &balance, status);
        trim(user);
        if (strcmp(user, username) == 0) {
            if (balance >= amount) {
                balance -= amount;
                fseek(fp, pos, SEEK_SET);
                fprintf(fp, "%s|%s|%s|%.2f|%s\n", user, accountnumber, mobilenumber, balance, status);
                fflush(fp);
                updated = true;
                close_file_locked(fp, fd);
                log_transaction(username, "Withdraw", amount, "Withdrawal from account");
                return true;
            } else {
                close_file_locked(fp, fd);
                return false; // insufficient balance
            }
        }
        pos = ftell(fp);
    }
    close_file_locked(fp, fd);
    return false; // user not found or no sufficient balance
}

void log_transfer_transaction(const char *sender_account, float amount, const char *receiver_account) {
    FILE *fp = fopen("transactions.txt", "a");
    if (!fp) {
        perror("Failed to open transactions.txt");
        return;
    }

    int fd = fileno(fp);
    // Lock file for append
    struct flock lock;
    memset(&lock, 0, sizeof(lock));
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;

    if (fcntl(fd, F_SETLKW, &lock) == -1) {
        perror("Failed to lock transactions.txt");
        fclose(fp);
        return;
    }

    // Write transaction line
    fprintf(fp, "%s,%.2f,%s\n", sender_account, amount, receiver_account);
    fflush(fp);

    // Unlock file
    lock.l_type = F_UNLCK;
    if (fcntl(fd, F_SETLKW, &lock) == -1) {
        perror("Failed to unlock transactions.txt");
    }

    fclose(fp);
}


bool transfer_funds(const char *from_account, const char *to_user, float amount) {
    if (amount <= 0) return false;
    if (!customer_exists(to_user)) return false;

    int fd;
    FILE *fp = open_file_locked(CUSTOMER_DATA_FILE, "r+", &fd);
    if (!fp) return false;

    char lines[100][256];  // Adjust size accordingly
    int count = 0;

    // Read all customer data lines into memory
    while (fgets(lines[count], sizeof(lines[count]), fp)) {
        count++;
        if (count >= 100) break; // safety limit
    }

    int sender_idx = -1, receiver_idx = -1;
    float sender_balance = 0.0f, receiver_balance = 0.0f;
    char user[64], accountnumber[32], mobilenumber[32], status[32];

    // Find sender (current user's account number) and receiver
    for (int i = 0; i < count; i++) {
        sscanf(lines[i], "%63[^|]|%31[^|]|%31[^|]|%f|%31[^\n]", user, accountnumber, mobilenumber, &sender_balance, status);
        trim(user);
        trim(accountnumber);
        if (strcmp(accountnumber, from_account) == 0) {
            sender_idx = i;
            break;
        }
    }

    for (int i = 0; i < count; i++) {
        sscanf(lines[i], "%63[^|]|%[^\n]", user, accountnumber);
        trim(user);
        trim(accountnumber);
        if (strcmp(user, to_user) == 0) {
            receiver_idx = i;
            break;
        }
    }

    if (sender_idx == -1 || receiver_idx == -1) {
        close_file_locked(fp, fd);
        return false; // sender or receiver not found
    }

    // Parse balances
    sscanf(lines[sender_idx], "%63[^|]|%f|%31[^\n]", user, &sender_balance, status);
    sscanf(lines[receiver_idx], "%63[^|]|%f|%31[^\n]", user, &receiver_balance, status);

    if (sender_balance < amount) {
        close_file_locked(fp, fd);
        return false; // insufficient funds
    }

    // Adjust balances
    sender_balance -= amount;
    receiver_balance += amount;

    // Update lines with new balances
    snprintf(lines[sender_idx], sizeof(lines[sender_idx]), "%s|%s|%s|%.2f|%s\n", user, from_account, mobilenumber, sender_balance, status);
    snprintf(lines[receiver_idx], sizeof(lines[receiver_idx]), "%s|%s|%s|%.2f|%s\n", user, accountnumber, mobilenumber, receiver_balance, status);

    // Write all lines back
    fseek(fp, 0, SEEK_SET);
    for (int i = 0; i < count; i++) {
        fputs(lines[i], fp);
    }
    fflush(fp);
    ftruncate(fd, ftell(fp)); // truncate to new size

    close_file_locked(fp, fd);

    return true;
}


bool apply_for_loan(const char *username, float loan_amount) {
    if (loan_amount <= 0) return false;

    // Open customer data to get account number of user
    FILE *fp_cust = fopen(CUSTOMER_DATA_FILE, "r");
    if (!fp_cust) return false;

    char line[256];
    char accountnumber[32] = {0};
    bool found = false;

    // Find user's account number
    while (fgets(line, sizeof(line), fp_cust)) {
        char user[64], mobile[32], status[32];
        float balance;
        sscanf(line, "%63[^|]|%31[^|]|%31[^|]|%f|%31[^\n]", user, accountnumber, mobile, &balance, status);
        trim(user);
        if (strcmp(user, username) == 0) {
            found = true;
            break;
        }
    }
    fclose(fp_cust);

    if (!found) return false;

    // Append new loan entry in loans.txt
    FILE *fp_loans = fopen("loans.txt", "a");
    if (!fp_loans) return false;

    // Generate new loan id, for example you can implement increment logic, here simplified:
    static int loan_id_counter = 1000; // maintain this globally or load from file on startup
    loan_id_counter++;

    // Write loan entry: loanid|accountnumber|amount|status|employeeid
    fprintf(fp_loans, "%d|%s|%.2f|In Progress|U000000\n", loan_id_counter, accountnumber, loan_amount);
    fclose(fp_loans);

    log_transaction(username, "Loan Application", loan_amount, "Loan requested");

    return true;
}


bool customer_change_password(const char *username, const char *new_password) {
    int fd;
    FILE *fp = open_file_locked("users.txt", "r+", &fd);
    if (!fp) return false;

    char line[256];
    long pos = 0;
    bool updated = false;
    while (fgets(line, sizeof(line), fp)) {
        char role[32], user[64], pass[64];
        sscanf(line, "%31[^|]|%63[^|]|%63[^\n]", role, user, pass);
        trim(user);
        if (strcmp(user, username) == 0) {
            fseek(fp, pos, SEEK_SET);
            fprintf(fp, "%s|%s|%s\n", role, user, new_password);
            fflush(fp);
            updated = true;
            log_transaction(username, "Password Change", 0.0f, "Changed password");
            break;
        }
        pos = ftell(fp);
    }
    close_file_locked(fp, fd);
    return updated;
}

bool add_feedback(const char *username, const char *feedback) {
    int fd;
    FILE *fp = fopen("feedback.txt", "a");
    if (!fp) return false;

    fd = fileno(fp);
    if (flock(fd, LOCK_EX) != 0) {
        fclose(fp);
        return false;
    }

    time_t now = time(NULL);
    char *dt = ctime(&now);
    dt[strlen(dt) - 1] = '\0'; // Remove newline at end

    fprintf(fp, "%s|%s|%s\n", username, dt, feedback);

    flock(fd, LOCK_UN);
    fclose(fp);
    return true;
}

bool transfer_funds_with_account(const char *from_account, const char *to_account, float amount) {
    if (amount <= 0) return false;

    int fd;
    FILE *fp = open_file_locked(CUSTOMER_DATA_FILE, "r+", &fd);
    if (!fp) return false;

    char lines[100][256];
    int count = 0;

    while (fgets(lines[count], sizeof(lines[count]), fp)) {
        count++;
        if (count >= 100) break; // safety limit
    }

    int sender_idx = -1, receiver_idx = -1;
    float sender_balance = 0.0f, receiver_balance = 0.0f;
    char user[64], accountnum[32], mobilenumber[32], status[32];

    for (int i = 0; i < count; i++) {
        sscanf(lines[i], "%63[^|]|%31[^|]|%31[^|]|%f|%31[^\n]", user, accountnum, mobilenumber, &sender_balance, status);
        trim(accountnum);
        if (strcmp(accountnum, from_account) == 0) sender_idx = i;
        if (strcmp(accountnum, to_account) == 0) receiver_idx = i;
    }

    if (sender_idx == -1 || receiver_idx == -1) {
        close_file_locked(fp, fd);
        return false;
    }

    sscanf(lines[sender_idx], "%63[^|]|%31[^|]|%31[^|]|%f|%31[^\n]", user, accountnum, mobilenumber, &sender_balance, status);
    sscanf(lines[receiver_idx], "%63[^|]|%31[^|]|%31[^|]|%f|%31[^\n]", user, accountnum, mobilenumber, &receiver_balance, status);

    if (sender_balance < amount) {
        close_file_locked(fp, fd);
        return false;
    }

    sender_balance -= amount;
    receiver_balance += amount;

    snprintf(lines[sender_idx], sizeof(lines[sender_idx]), "%s|%s|%s|%.2f|%s\n", user, from_account, mobilenumber, sender_balance, status);
    snprintf(lines[receiver_idx], sizeof(lines[receiver_idx]), "%s|%s|%s|%.2f|%s\n", user, to_account, mobilenumber, receiver_balance, status);

    fseek(fp, 0, SEEK_SET);
    for (int i = 0; i < count; i++) {
        fputs(lines[i], fp);
    }
    fflush(fp);
    ftruncate(fd, ftell(fp));

    close_file_locked(fp, fd);

    return true;
}

bool get_transaction_history(const char *username, char *output_buffer, size_t buf_size) {
    if (!username || !output_buffer) return false;

    FILE *fp = fopen("transactions.log", "r");
    if (!fp) return false;

    output_buffer[0] = '\0';

    char line[512];
    char user_in_line[128];

    while (fgets(line, sizeof(line), fp)) {
        // Extract username from line (assuming format: username|transaction_type|amount|date|desc)
        if (sscanf(line, "%127[^|]|", user_in_line) == 1) {
            // Trim newline/whitespace
            for (int i = 0; user_in_line[i]; i++) {
                if (user_in_line[i] == '\n' || user_in_line[i] == '\r') {
                    user_in_line[i] = '\0';
                    break;
                }
            }
            if (strcmp(user_in_line, username) == 0) {
                // Append line safely to output_buffer
                if ((strlen(output_buffer) + strlen(line) + 1) < buf_size) {
                    strcat(output_buffer, line);
                } else {
                    // Buffer full; stop reading more
                    break;
                }
            }
        }
    }

    fclose(fp);
    return (strlen(output_buffer) > 0);
}


