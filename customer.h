#ifndef CUSTOMER_H
#define CUSTOMER_H
#include <stddef.h>
#include <stdbool.h>

bool validate_customer_login(const char *username, const char *password);
bool customer_exists(const char *username);
bool add_customer(const char *username, const char *password);
float get_account_balance(const char *username);
bool deposit_money(const char *username, float amount);
void log_transaction(const char *username, const char *type, float amount, const char *remarks);
bool withdraw_money(const char *username, float amount);
bool transfer_funds(const char *from_user, const char *to_user, float amount);
bool apply_for_loan(const char *username, float amount);
bool change_password(const char *username, const char *new_password);
bool add_feedback(const char *username, const char *feedback);
bool get_transaction_history(const char *username, char *output_buffer, size_t buf_size);


#endif
