#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int main() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE] = {0};
    
    // Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    printf("[SUCCESS] Socket created successfully\n");
    
    // Set socket options to reuse address
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Setsockopt failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    
    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;  // Accept connections from any IP
    server_addr.sin_port = htons(PORT);
    
    // Bind socket to address and port
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    printf("[SUCCESS] Socket bound to port %d\n", PORT);
    
    // Listen for incoming connections (max 5 queued connections)
    if (listen(server_fd, 5) < 0) {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    printf("[LISTENING] Server is listening on port %d...\n", PORT);
    
    // Accept client connection
    client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
    if (client_fd < 0) {
        perror("Accept failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    
    printf("[CONNECTED] Client connected from %s:%d\n", 
           inet_ntoa(client_addr.sin_addr), 
           ntohs(client_addr.sin_port));
    
    // Communication loop
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        
        // Receive message from client
        int bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            printf("[DISCONNECTED] Client disconnected\n");
            break;
        }
        
        printf("[CLIENT] %s\n", buffer);
        
        // Check for exit command
        if (strcmp(buffer, "quit") == 0 || strcmp(buffer, "exit") == 0) {
            printf("[INFO] Client requested to close connection\n");
            break;
        }
        
        // Send response to client
        char response[BUFFER_SIZE];
        snprintf(response, BUFFER_SIZE, "Server received: %s", buffer);
        send(client_fd, response, strlen(response), 0);
        printf("[SENT] Response sent to client\n");
    }
    
    // Close connections
    close(client_fd);
    close(server_fd);
    printf("[CLOSED] Server shutdown\n");
    
    return 0;
}