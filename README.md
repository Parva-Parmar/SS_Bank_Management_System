# SS_Bank_Management_System


# Banking Management System — Client Program

## Overview

The Client program is the entry point for the **Banking Management System** project.  
It acts as an interface between the user and the server, allowing users to log in, sign up, and interact with the banking system.  

The client connects to the server using **TCP sockets** and supports two-way communication via **multi-threading**.

## Key Features

This client program is implemented in **C** and demonstrates:

- **Socket Programming** — Establishes connection with the server using TCP sockets.
- **Multi-threading** — Enables asynchronous communication between client and server.
- **Role-based Login/Signup** — Supports different user roles with authentication.
- **Command-based Communication** — Allows the client to send and receive commands from the server.

# Banking Management System — Client Program

## Overview

The Client program is the entry point for the **Banking Management System** project.  
It acts as an interface between the user and the server, allowing users to log in, sign up, and interact with the banking system.  

The client connects to the server using **TCP sockets** and supports two-way communication via **multi-threading**.

## Key Features

This client program is implemented in **C** and demonstrates:

- **Socket Programming** — Establishes connection with the server using TCP sockets.  
- **Multi-threading** — Enables asynchronous communication between client and server.  
- **Role-based Login/Signup** — Supports different user roles with authentication.  
- **Command-based Communication** — Allows the client to send and receive commands from the server.  

---

## Features in Detail

### 1. Connection to the Server
- Uses a **TCP socket** to connect to the server at `127.0.0.1` on **port 8080**.  
- Displays a connection status message when the connection is successful.  

---

### 2. Landing Menu
Displays a menu for user actions:

===== Banking Management System =====
1. Login
2. Signup (Manager/Employee only)
3. Exit
Enter choice:


---

### 3. Login Functionality
- Prompts the user to input:
  - **Username**
  - **Password**
  - **Role** (Customer, Employee, Manager)

- Sends a login request to the server in the format:  

LOGIN|username|password|role


- Processes server responses:
- `LOGIN_SUCCESS|role` → Login successful.  
- `LOGIN_FAIL` → Invalid credentials or role mismatch.  
- `LOGIN_FAIL: Already logged in` → Session already active for this user.  

---

### 4. Signup Functionality
- Prompts for:
- **New Username**
- **New Password**
- **Role selection** (Employee or Manager only)

- Sends a signup request in the format:  

SIGNUP|username|password|role


- Processes server responses:
- `SIGNUP_SUCCESS` → Account created successfully.  
- `SIGNUP_FAIL: Only Manager or Employee allowed` → Role restriction violation.  

---

### 5. Exit Functionality
- Sends `"exit"` command to the server.  
- Closes the connection and terminates the client program.  

---

### 6. Two-Way Communication
- Uses a dedicated thread (`receiveMessages`) to continuously **listen for server messages**.  
- The **main thread** handles user input and sends commands to the server.  

## Program Flow

1. The client starts and attempts to connect to the server.  
2. Upon successful connection, the **landing menu** is displayed.  
3. The user chooses one of the following options:
   - **Login** → Provide credentials and role, then send to server.  
   - **Signup** → Provide username, password, and role (Employee/Manager only), then send to server.  
   - **Exit** → Send `"exit"` command and close the connection.  
4. The client listens for server responses and displays them in **real-time**.  

---

## Code Highlights

### Multi-threaded Communication
- A dedicated thread (`receiveMessages`) continuously listens for messages from the server.  
- The main thread handles **user input** and sends commands to the server.  
- This ensures **asynchronous, real-time communication** without blocking user interaction.  

```c
void* receiveMessages(void* socket_fd) {
    int sock = *(int*)socket_fd;
    char buffer[1024];
    while (1) {
        memset(buffer, 0, sizeof(buffer));
        int valread = read(sock, buffer, sizeof(buffer));
        if (valread <= 0) break;
        printf("%s\n", buffer);
        if (strcmp(buffer, "exit") == 0) break;
    }
    pthread_exit(NULL);
}
```

## Landing Menu

The client displays the following landing menu to guide user actions:

```c
void showLandingMenu() {
    printf("\n===== Banking Management System =====\n");
    printf("1. Login\n");
    printf("2. Signup (Manager/Employee only)\n");
    printf("3. Exit\n");
    printf("Enter choice: ");
}

```