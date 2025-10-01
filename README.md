# SS_Bank_Management_System

## Banking Management System — Client Program

### Overview
The Client program is the entry point for the **Banking Management System** project.  
It acts as an interface between the user and the server, allowing users to **log in, sign up, and interact** with the banking system.  

The client connects to the server using **TCP sockets** and supports **two-way communication** via multi-threading.

---

### Key Features
This client program is implemented in **C** and demonstrates:

- **Socket Programming** — Establishes a TCP connection with the server.  
- **Multi-threading** — Enables asynchronous, non-blocking communication.  
- **Role-based Login/Signup** — Supports different user roles with authentication.  
- **Command-based Communication** — Sends commands to the server and processes responses.  

---

## Features in Detail

### 1. Connection to the Server
- Connects to the server at `127.0.0.1` on **port 8080**.  
- Displays a connection status message when successful.  

---

### 2. Landing Menu
The client displays a menu for user interaction:



===== Banking Management System =====
1. Login
2. Signup (Manager/Employee only)
3. Exit
Enter choice:



---

### 3. Login Functionality
- Prompts the user to enter:
  - **Username**
  - **Password**
  - **Role** (Customer, Employee, Manager)

- Sends the request in the format:  


LOGIN|username|password|role


- Processes server responses:
- `LOGIN_SUCCESS|role` → Login successful.  
- `LOGIN_FAIL` → Invalid credentials or role mismatch.  
- `LOGIN_FAIL: Already logged in` → Session already active.  

---

### 4. Signup Functionality
- Prompts for:
- **New Username**
- **New Password**
- **Role selection** (Employee or Manager only)

- Sends the request in the format:  


SIGNUP|username|password|role


- Processes server responses:
- `SIGNUP_SUCCESS` → Account created successfully.  
- `SIGNUP_FAIL: Username already exists` → Username is already taken.  
- `SIGNUP_FAIL: Only Manager or Employee allowed` → Role restriction violation.  

---

### 5. Exit Functionality
- Sends `"exit"` command to the server.  
- Closes the connection and terminates the program.  

---

### 6. Two-Way Communication
- A **dedicated thread** (`receiveMessages`) continuously listens for messages from the server.  
- The **main thread** handles user input and sends commands.  
- This design ensures **asynchronous, real-time communication**.  

---

## Program Flow

1. Client starts and attempts to connect to the server.  
2. Upon successful connection, the **landing menu** is displayed.  
3. User chooses one of the following:
 - **Login** → Provide credentials and role.  
 - **Signup** → Provide username, password, and role (restricted to Employee/Manager).  
 - **Exit** → Send `"exit"` and close connection.  
4. Client listens for server responses and displays them in **real-time**.  

---

## Code Highlights

### Multi-threaded Communication
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