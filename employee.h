#ifndef EMPLOYEE_H
#define EMPLOYEE_H

int validate_employee_login(const char *username, const char *password);
int add_new_customer(const char *username, const char *password, const char *account_number, const char *mobile);
void modify_customer_details(int new_socket);
void process_loan_applications(int new_socket, const char *employee_id);
void approve_loan(int new_socket);
void reject_loan(int new_socket);
void view_assigned_loan_applications(int new_socket);
void view_customer_transactions(int new_socket);
void change_password(int new_socket);

#endif // EMPLOYEE_H
