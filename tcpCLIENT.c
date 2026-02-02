#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define SERVER_IP "127.0.0.1"

int main() {
    int client_fd;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE] = {0};
    char message[BUFFER_SIZE];
    
    // Create socket
    client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    printf("[SUCCESS] Socket created successfully\n");
    
    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    
    // Convert IP address from text to binary
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("Invalid address or address not supported");
        close(client_fd);
        exit(EXIT_FAILURE);
    }
    
    // Connect to server
    if (connect(client_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(client_fd);
        exit(EXIT_FAILURE);
    }
    printf("[CONNECTED] Connected to server at %s:%d\n", SERVER_IP, PORT);
    printf("Type 'quit' or 'exit' to close connection\n\n");
    
    // Communication loop
    while (1) {
        printf("Enter message: ");
        fgets(message, BUFFER_SIZE, stdin);
        
        // Remove newline character
        message[strcspn(message, "\n")] = 0;
        
        // Send message to server
        send(client_fd, message, strlen(message), 0);
        printf("[SENT] Message sent to server\n");
        
        // Check for exit command
        if (strcmp(message, "quit") == 0 || strcmp(message, "exit") == 0) {
            printf("[INFO] Closing connection...\n");
            break;
        }
        
        // Clear buffer
        memset(buffer, 0, BUFFER_SIZE);
        
        // Receive response from server
        int bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            printf("[DISCONNECTED] Server closed connection\n");
            break;
        }
        
        printf("[SERVER] %s\n\n", buffer);
    }
    
    // Close connection
    close(client_fd);
    printf("[CLOSED] Connection closed\n");
    
    return 0;
}