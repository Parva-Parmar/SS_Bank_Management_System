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
        char user[64], loan_status[32];
        float bal;
        sscanf(line, "%63[^|]|%f|%31[^\n]", user, &bal, loan_status);
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
        char user[64], loan_status[32];
        float balance;
        sscanf(line, "%63[^|]|%f|%31[^\n]", user, &balance, loan_status);
        trim(user);
        if (strcmp(user, username) == 0) {
            balance += amount;
            fseek(fp, pos, SEEK_SET);
            fprintf(fp, "%s|%.2f|%s\n", user, balance, loan_status);
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


// Placeholder function definitions for the rest (you can implement similarly)
bool withdraw_money(const char *username, float amount) { return false; }
bool transfer_funds(const char *from_user, const char *to_user, float amount) { return false; }
bool apply_for_loan(const char *username, float amount) { return false; }
bool change_password(const char *username, const char *new_password) { return false; }
bool add_feedback(const char *username, const char *feedback) { return false; }
void view_transaction_history(const char *username, int sockfd) {}
