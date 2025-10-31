#ifndef EMPLOYEE_H
#define EMPLOYEE_H
#include <stdbool.h>
int validate_employee_login(const char *username, const char *password);
int add_new_customer(const char *username, const char *password, const char *account_number, const char *mobile);
int modify_customer_details(const char *username, const char *new_password, const char *new_mobile);
void process_loan_applications1(int new_socket, const char *employee_id);
void process_loan_applications2(int new_socket, const char *employee_id);
int reject_loan_by_id(const char *loan_id_to_reject, const char *employeeid);
int approve_loan_by_id(const char *loan_id_to_approve, const char *employeeid);
bool employee_change_password(const char *username, const char *new_password);
// void approve_loan(int new_socket, const char *employee_id);
// void reject_loan(int new_socket, const char *employee_id);
// void view_customer_transactions(int new_socket);
// void change_password(int new_socket);

#endif // EMPLOYEE_H
