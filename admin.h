#ifndef ADMIN_H
#define ADMIN_H

#include <pthread.h>
#include <stdbool.h>

// Function declarations for admin operations

// Initialize admin module resources if any
void admin_init(pthread_mutex_t *log_mutex);
int validate_admin_login(const char *username, const char *password);
// Add new employee (role, username, password, mobile)
int add_new_employee(const char *role, const char *username, const char *password, const char *mobile);

// Modify user info (username, field to modify, new value)
int modify_user_details(const char *user_type, const char *username, const char *new_password, const char *new_mobile);

// Change admin password (username is "admin")
int change_user_role(const char *username);
bool admin_change_password(const char *username, const char *new_password);
#endif
