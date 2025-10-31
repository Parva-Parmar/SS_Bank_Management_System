#ifndef MANAGER_H
#define MANAGER_H

#include <stdbool.h>
int validate_manager_login(const char *username, const char *password);
// Activate or deactivate a customer account
bool manager_set_customer_account_status(const char *accountnum, const char *status);

// Assign loan application process to an employee
int assign_loan_to_employee(const char *loanID, const char *employeeID);

// Review customer feedback (just a placeholder for now)
void manager_review_customer_feedback(void);

// Change manager's password
bool change_password(const char *username, const char *new_password);

// Logout manager session (placeholder)
void manager_logout(void);

#endif // MANAGER_H
