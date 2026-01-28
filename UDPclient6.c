#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include<pthread.h>

#define SERVER_PORT 12345
#define SERVER_IP "127.0.0.1"
#define BUFFER_SIZE 1024

// Global variables
int client_socket;
struct sockaddr_in server_addr;
int running = 1;

// Function to receive messages from server
void *receive_messages(void *arg) {
    char buffer[BUFFER_SIZE];
    socklen_t server_len = sizeof(server_addr);
    
    while (running) {
        memset(buffer, 0, BUFFER_SIZE);
        
        // Receive data from server
        int recv_len = recvfrom(client_socket, buffer, BUFFER_SIZE - 1, 0,
                               (struct sockaddr *)&server_addr, &server_len);
        
        if (recv_len > 0) {
            buffer[recv_len] = '\0';
            printf("\n[Server]: %s\n", buffer);
            printf("Client> ");
            fflush(stdout);
        } else if (recv_len == 0) {
            printf("\nServer disconnected.\n");
            running = 0;
            break;
        }
    }
    
    return NULL;
}

// Function to send messages to server
void *send_messages(void *arg) {
    char message[BUFFER_SIZE];
    
    // Send initial connection message
    char hello[] = "Hello from client!";
    sendto(client_socket, hello, strlen(hello), 0,
           (struct sockaddr *)&server_addr, sizeof(server_addr));
    
    while (running) {
        printf("Client> ");
        fflush(stdout);
        
        if (fgets(message, BUFFER_SIZE, stdin) == NULL) {
            break;
        }
        
        // Remove newline character
        message[strcspn(message, "\n")] = 0;
        
        // Check for quit/disconnect command
        if (strcmp(message, "quit") == 0 || strcmp(message, "QUIT") == 0 ||
            strcmp(message, "disconnect") == 0 || strcmp(message, "DISCONNECT") == 0) {
            
            // Send disconnect message to server
            sendto(client_socket, "disconnect", strlen("disconnect"), 0,
                   (struct sockaddr *)&server_addr, sizeof(server_addr));
            
            printf("Disconnecting from server...\n");
            running = 0;
            break;
        }
        
        // Send message to server
        if (strlen(message) > 0) {
            if (sendto(client_socket, message, strlen(message), 0,
                      (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
                perror("Send failed");
            }
        }
    }
    
    return NULL;
}

int main() {
    pthread_t recv_thread, send_thread;
    
    // Create UDP socket
    client_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (client_socket < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    
    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    
    // Convert IP address from string to binary
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("Invalid address");
        close(client_socket);
        exit(EXIT_FAILURE);
    }
    
    printf("==============================================\n");
    printf("UDP Client started\n");
    printf("Connecting to server %s:%d\n", SERVER_IP, SERVER_PORT);
    printf("Commands: 'quit' or 'disconnect' - exit\n");
    printf("==============================================\n\n");
    
    // Create threads for receiving and sending
    if (pthread_create(&recv_thread, NULL, receive_messages, NULL) != 0) {
        perror("Failed to create receive thread");
        close(client_socket);
        exit(EXIT_FAILURE);
    }
    
    if (pthread_create(&send_thread, NULL, send_messages, NULL) != 0) {
        perror("Failed to create send thread");
        close(client_socket);
        exit(EXIT_FAILURE);
    }
    
    // Wait for threads to finish
    pthread_join(send_thread, NULL);
    
    // Cancel receive thread and close socket
    pthread_cancel(recv_thread);
    close(client_socket);
    
    printf("Client disconnected.\n");
    
    return 0;
}