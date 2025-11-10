#include "customer.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>
#include <time.h>
#include "filelock.h"
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

void log_transaction(const char *accountnumber, const char *type, float amount, const char *remarks) {
    int fd;
    FILE *fp = open_file_locked(TRANSACTIONS_FILE, "a", &fd);
    if (!fp)
        return;
    time_t now = time(NULL);
    char *dt = ctime(&now);
    dt[strlen(dt) - 1] = '\0';
    fprintf(fp, "%s|%s|%.2f|%s|%s\n", accountnumber, type, amount, dt, remarks);
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
            
            // Password and username matched for customer role, now check status
            fclose(fp);

            // Open customer_data.txt to check status
            FILE *cfp = fopen("customer_data.txt", "r");
            if (!cfp)
                return false;

            char cline[256];
            while (fgets(cline, sizeof(cline), cfp)) {
                char cusername[64], accountnumber[32], mobilenumber[32], balance[32], status[32];
                sscanf(cline, "%63[^|]|%31[^|]|%31[^|]|%31[^|]|%31[^\n]", cusername, accountnumber, mobilenumber, balance, status);
                trim(cusername);
                trim(status);
                if (strcmp(username, cusername) == 0) {
                    fclose(cfp);
                    if (strcmp(status, "Active") == 0)
                        return true;  // Login allowed only if Active
                    else
                        return false; // Status not active
                }
            }
            fclose(cfp);
            return false; // Username not found in customer_data.txt
        }
    }
    fclose(fp);
    return false; // Username/password/role didn't match
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
    if (!fp) {
        perror("Failed to open customer data file");
        return -1.0f;  // Indicate failure
    }

    char line[256];
    float balance = -1.0f;

    while (fgets(line, sizeof(line), fp)) {
        char user[64], accountnumber[32], mobile[32], status[32];
        float bal;

        int parsed = sscanf(line, "%63[^|]|%31[^|]|%31[^|]|%f|%31[^\n]", user, accountnumber, mobile, &bal, status);
        if (parsed == 5) {
            trim(user);
            trim(status);

            if (strcmp(username, user) == 0) {
                if (strcmp(status, "Active") == 0) {
                    balance = bal;
                } else {
                    balance = -2.0f;  // Indicate inactive account with special code
                }
                break;
            }
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
    FILE *fp = open_file_locked(CUSTOMER_DATA_FILE, "r", &fd);
    if (!fp) {
        perror("Failed to open and lock customer data file for reading");
        return false;
    }

    char lines[1000][256];  // Adjust max lines if needed
    int count = 0;
    bool updated = false;
    int i;  // Declare here for use after loop

    // Read all lines to memory
    while (fgets(lines[count], sizeof(lines[count]), fp) && count < 1000) {
        count++;
    }
    close_file_locked(fp, fd);

    // Update balance in memory
    for (i = 0; i < count; i++) {
        char user[64], accountnumber[32], mobilenumber[32], status[32];
        float balance;
        int parsed = sscanf(lines[i], "%63[^|]|%31[^|]|%31[^|]|%f|%31[^\n]", user, accountnumber, mobilenumber, &balance, status);
        if (parsed == 5) {
            trim(user);
            trim(status);
            if (strcmp(user, username) == 0) {
                balance += amount;
                snprintf(lines[i], sizeof(lines[i]), "%s|%s|%s|%.2f|%s\n", user, accountnumber, mobilenumber, balance, status);
                updated = true;
                break;
            }
        }
    }

    if (!updated) {
        // User not found
        return false;
    }

    // Open file again for writing with locking
    fp = open_file_locked(CUSTOMER_DATA_FILE, "w", &fd);
    if (!fp) {
        perror("Failed to open and lock customer data file for writing");
        return false;
    }

    // Write updated data
    for (int j = 0; j < count; j++) {
        fputs(lines[j], fp);
    }
    close_file_locked(fp, fd);

    // Extract account number from updated line for logging
    char accountnumber[32] = {0};
    sscanf(lines[i], "%*[^|]|%31[^|]", accountnumber);
    log_transaction(accountnumber, "Deposit", amount, "Deposit to account");

    return true;
}




bool withdraw_money(const char *username, float amount) {
    if (amount <= 0) return false;

    FILE *fp = fopen(CUSTOMER_DATA_FILE, "r");
    if (!fp) return false;

    // Buffer for lines and updated content
    char lines[1000][512];  // assume max 1000 lines and max line length 512
    int count = 0;
    char acno[32];
    bool updated = false;

    // Read all lines into memory
    while (fgets(lines[count], sizeof(lines[count]), fp) && count < 1000) {
        count++;
    }
    fclose(fp);

    // Modify the line in memory
    for (int i = 0; i < count; i++) {
        char user[64], accountnumber[32], mobilenumber[32], status[32];
        float balance;
        sscanf(lines[i], "%63[^|]|%31[^|]|%31[^|]|%f|%31[^\n]", user, accountnumber, mobilenumber, &balance, status);
        trim(user);
        trim(status);

        if (strcmp(user, username) == 0) {
            if (balance >= amount) {
                balance -= amount;
                snprintf(lines[i], sizeof(lines[i]), "%s|%s|%s|%.2f|%s\n", user, accountnumber, mobilenumber, balance, status);
                strcpy(acno, accountnumber);
                updated = true;
                break;
            } else {
                return false; // insufficient balance
            }
        }
    }

    if (!updated) return false; // user not found or no sufficient balance

    // Write back all lines to file
    fp = fopen(CUSTOMER_DATA_FILE, "w");
    if (!fp) return false;

    for (int i = 0; i < count; i++) {
        fputs(lines[i], fp);
    }
    fclose(fp);

    log_transaction(acno, "Withdraw", amount, "Withdrawal from account");
    return true;
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

    // Open customer data file with read lock
    FILE *fp_cust = fopen(CUSTOMER_DATA_FILE, "r");
    if (!fp_cust) return false;
    int fd_cust = fileno(fp_cust);
    if (lock_file(fd_cust, F_RDLCK) != 0) {
        fclose(fp_cust);
        return false;
    }

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
    unlock_file(fd_cust);
    fclose(fp_cust);

    if (!found) return false;

    // Open loans.txt for reading to find max existing loan id
    FILE *fp_loans_read = fopen("loans.txt", "r");
    int max_loan_id = 1000; // Starting baseline
    if (fp_loans_read) {
        char loan_line[256];
        while (fgets(loan_line, sizeof(loan_line), fp_loans_read)) {
            int loan_id;
            // Loan file format: loanid|accountnumber|amount|status|employeeid
            if (sscanf(loan_line, "%d|%*[^|]|%*f|%*[^|]|%*s", &loan_id) == 1) {
                if (loan_id > max_loan_id) max_loan_id = loan_id;
            }
        }
        fclose(fp_loans_read);
    }
    max_loan_id++; // Next loan ID to use

    // Open loans.txt for append with write lock
    FILE *fp_loans = fopen("loans.txt", "a");
    if (!fp_loans) return false;
    int fd_loans = fileno(fp_loans);
    if (lock_file(fd_loans, F_WRLCK) != 0) {
        fclose(fp_loans);
        return false;
    }

    // Write the new loan entry
    fprintf(fp_loans, "%d|%s|%.2f|In Progress|U000000\n", max_loan_id, accountnumber, loan_amount);

    fflush(fp_loans);
    unlock_file(fd_loans);
    fclose(fp_loans);

    log_transaction(accountnumber, "Loan Application", loan_amount, "Loan requested");

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
    if (amount <= 0)
        return false;

    int fd;
    FILE *fp = open_file_locked(CUSTOMER_DATA_FILE, "r+", &fd);
    if (!fp)
        return false;

    char lines[100][256];
    int count = 0;

    // Read all lines into memory
    while (fgets(lines[count], sizeof(lines[count]), fp)) {
        count++;
        if (count >= 100) break; // safety limit
    }

    int sender_idx = -1, receiver_idx = -1;

    // Find indices of sender and receiver
    for (int i = 0; i < count; i++) {
        char user[64], accountnum[32], mobilenumber[32], status[32];
        float balance;
        int parsed = sscanf(lines[i], "%63[^|]|%31[^|]|%31[^|]|%f|%31[^\n]", user, accountnum, mobilenumber, &balance, status);
        trim(accountnum);
        if (parsed == 5) {
            if (strcmp(accountnum, from_account) == 0)
                sender_idx = i;
            if (strcmp(accountnum, to_account) == 0)
                receiver_idx = i;
        }
    }

    if (sender_idx == -1 || receiver_idx == -1) {
        close_file_locked(fp, fd);
        return false;
    }

    // Separate buffers for sender and receiver data
    char sender_user[64], sender_accountnum[32], sender_mobilenumber[32], sender_status[32];
    float sender_balance = 0.0f;

    char receiver_user[64], receiver_accountnum[32], receiver_mobilenumber[32], receiver_status[32];
    float receiver_balance = 0.0f;

    sscanf(lines[sender_idx], "%63[^|]|%31[^|]|%31[^|]|%f|%31[^\n]",
           sender_user, sender_accountnum, sender_mobilenumber, &sender_balance, sender_status);

    sscanf(lines[receiver_idx], "%63[^|]|%31[^|]|%31[^|]|%f|%31[^\n]",
           receiver_user, receiver_accountnum, receiver_mobilenumber, &receiver_balance, receiver_status);

    if (sender_balance < amount) {
        close_file_locked(fp, fd);
        return false;
    }

    // Update balances
    sender_balance -= amount;
    receiver_balance += amount;

    // Update sender line
    snprintf(lines[sender_idx], sizeof(lines[sender_idx]), "%s|%s|%s|%.2f|%s\n",
             sender_user, from_account, sender_mobilenumber, sender_balance, sender_status);

    // Update receiver line
    snprintf(lines[receiver_idx], sizeof(lines[receiver_idx]), "%s|%s|%s|%.2f|%s\n",
             receiver_user, to_account, receiver_mobilenumber, receiver_balance, receiver_status);

    // Write all lines back to file
    fseek(fp, 0, SEEK_SET);
    for (int i = 0; i < count; i++) {
        fputs(lines[i], fp);
    }
    fflush(fp);
    ftruncate(fd, ftell(fp));

    close_file_locked(fp, fd);

    log_transaction(from_account, "Transfer", amount, to_account);
    return true;
}


bool get_transaction_history(const char *username, char *output_buffer, size_t buf_size) {
    if (!username || !output_buffer) return false;

    // Open customer data file with shared read lock
    FILE *fp_cust = fopen(CUSTOMER_DATA_FILE, "r");
    if (!fp_cust) return false;
    int fd_cust = fileno(fp_cust);
    if (lock_file(fd_cust, F_RDLCK) != 0) {
        fclose(fp_cust);
        return false;
    }

    char line[256];
    char file_user[64], accountnumber[32] = {0}, mobile[32], status[32];
    float balance;
    bool found = false;

    // Find accountnumber for username
    while (fgets(line, sizeof(line), fp_cust)) {
        int parsed = sscanf(line, "%63[^|]|%31[^|]|%31[^|]|%f|%31[^\n]", file_user, accountnumber, mobile, &balance, status);
        if (parsed == 5) {
            trim(file_user);
            if (strcmp(file_user, username) == 0) {
                found = true;
                break;
            }
        }
    }

    unlock_file(fd_cust);
    fclose(fp_cust);

    if (!found) return false;

    // Open transactions.txt with shared read lock
    FILE *fp_tx = fopen("transactions.txt", "r");
    if (!fp_tx) return false;
    int fd_tx = fileno(fp_tx);
    if (lock_file(fd_tx, F_RDLCK) != 0) {
        fclose(fp_tx);
        return false;
    }

    output_buffer[0] = '\0';
    char tx_line[512], acc_in_tx[64];

    // Read lines matching accountnumber
    while (fgets(tx_line, sizeof(tx_line), fp_tx)) {
        if (sscanf(tx_line, "%63[^|]|", acc_in_tx) == 1) {
            trim(acc_in_tx);
            if (strcmp(acc_in_tx, accountnumber) == 0) {
                if ((strlen(output_buffer) + strlen(tx_line) + 1) < buf_size) {
                    strcat(output_buffer, tx_line);
                } else {
                    break;  // buffer full
                }
            }
        }
    }

    unlock_file(fd_tx);
    fclose(fp_tx);

    return (strlen(output_buffer) > 0);
}


