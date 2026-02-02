#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int client_socket;

// Thread function to receive messages from client
void *receive_messages(void *arg) {
    char buffer[BUFFER_SIZE];
    int bytes_received;
    
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        
        if (bytes_received <= 0) {
            printf("\nClient disconnected.\n");
            close(client_socket);
            exit(0);
        }
        
        printf("\nClient: %s", buffer);
        printf("Server: ");
        fflush(stdout);
    }
    
    return NULL;
}

// Thread function to send messages to client
void *send_messages(void *arg) {
    char buffer[BUFFER_SIZE];
    
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        fgets(buffer, BUFFER_SIZE, stdin);
        
        if (send(client_socket, buffer, strlen(buffer), 0) < 0) {
            printf("Send failed\n");
            close(client_socket);
            exit(1);
        }
        
        // Check for exit command
        if (strncmp(buffer, "exit", 4) == 0) {
            printf("Closing connection...\n");
            close(client_socket);
            exit(0);
        }
    }
    
    return NULL;
}

int main() {
    int server_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    pthread_t recv_thread, send_thread;
    
    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Socket creation failed");
        exit(1);
    }
    
    // Set socket options to reuse address
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Setsockopt failed");
        exit(1);
    }
    
    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    
    // Bind socket
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_socket);
        exit(1);
    }
    
    // Listen for connections
    if (listen(server_socket, 1) < 0) {
        perror("Listen failed");
        close(server_socket);
        exit(1);
    }
    
    printf("Server started on port %d\n", PORT);
    printf("Waiting for client connection...\n");
    
    // Accept connection
    client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_addr_len);
    if (client_socket < 0) {
        perror("Accept failed");
        close(server_socket);
        exit(1);
    }
    
    printf("Client connected: %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    printf("You can start chatting now. Type 'exit' to quit.\n");
    printf("Server: ");
    fflush(stdout);
    
    // Create threads for sending and receiving
    if (pthread_create(&recv_thread, NULL, receive_messages, NULL) != 0) {
        perror("Thread creation failed");
        close(client_socket);
        close(server_socket);
        exit(1);
    }
    
    if (pthread_create(&send_thread, NULL, send_messages, NULL) != 0) {
        perror("Thread creation failed");
        close(client_socket);
        close(server_socket);
        exit(1);
    }
    
    // Wait for threads to finish
    pthread_join(recv_thread, NULL);
    pthread_join(send_thread, NULL);
    
    close(server_socket);
    close(client_socket);
    
    return 0;
}