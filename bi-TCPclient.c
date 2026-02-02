#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int server_socket;

// Thread function to receive messages from server
void *receive_messages(void *arg) {
    char buffer[BUFFER_SIZE];
    int bytes_received;
    
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        bytes_received = recv(server_socket, buffer, BUFFER_SIZE - 1, 0);
        
        if (bytes_received <= 0) {
            printf("\nServer disconnected.\n");
            close(server_socket);
            exit(0);
        }
        
        printf("\nServer: %s", buffer);
        printf("Client: ");
        fflush(stdout);
    }
    
    return NULL;
}

// Thread function to send messages to server
void *send_messages(void *arg) {
    char buffer[BUFFER_SIZE];
    
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        fgets(buffer, BUFFER_SIZE, stdin);
        
        if (send(server_socket, buffer, strlen(buffer), 0) < 0) {
            printf("Send failed\n");
            close(server_socket);
            exit(1);
        }
        
        // Check for exit command
        if (strncmp(buffer, "exit", 4) == 0) {
            printf("Closing connection...\n");
            close(server_socket);
            exit(0);
        }
    }
    
    return NULL;
}

int main(int argc, char *argv[]) {
    struct sockaddr_in server_addr;
    pthread_t recv_thread, send_thread;
    char *server_ip = "127.0.0.1";
    
    // Allow custom server IP as command line argument
    if (argc > 1) {
        server_ip = argv[1];
    }
    
    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Socket creation failed");
        exit(1);
    }
    
    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    
    // Convert IP address from string to binary
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("Invalid address");
        close(server_socket);
        exit(1);
    }
    
    // Connect to server
    printf("Connecting to server %s:%d...\n", server_ip, PORT);
    if (connect(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(server_socket);
        exit(1);
    }
    
    printf("Connected to server successfully!\n");
    printf("You can start chatting now. Type 'exit' to quit.\n");
    printf("Client: ");
    fflush(stdout);
    
    // Create threads for sending and receiving
    if (pthread_create(&recv_thread, NULL, receive_messages, NULL) != 0) {
        perror("Thread creation failed");
        close(server_socket);
        exit(1);
    }
    
    if (pthread_create(&send_thread, NULL, send_messages, NULL) != 0) {
        perror("Thread creation failed");
        close(server_socket);
        exit(1);
    }
    
    // Wait for threads to finish
    pthread_join(recv_thread, NULL);
    pthread_join(send_thread, NULL);
    
    close(server_socket);
    
    return 0;
}