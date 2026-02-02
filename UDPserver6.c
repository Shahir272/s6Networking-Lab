#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define PORT 12345
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 10

// Structure to hold client information
typedef struct {
    struct sockaddr_in address;
    int is_active;
} Client;

// Global variables
int server_socket;
Client clients[MAX_CLIENTS];
int client_count = 0;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

// Function to add a new client
void add_client(struct sockaddr_in client_addr) {
    pthread_mutex_lock(&clients_mutex);
    
    // Check if client already exists
    for (int i = 0; i < client_count; i++) {
        if (clients[i].address.sin_addr.s_addr == client_addr.sin_addr.s_addr &&
            clients[i].address.sin_port == client_addr.sin_port) {
            pthread_mutex_unlock(&clients_mutex);
            return;
        }
    }
    
    // Add new client
    if (client_count < MAX_CLIENTS) {
        clients[client_count].address = client_addr;
        clients[client_count].is_active = 1;
        client_count++;
        printf("\n[NEW CLIENT] %s:%d connected\n", 
               inet_ntoa(client_addr.sin_addr), 
               ntohs(client_addr.sin_port));
        printf("Total clients: %d\n", client_count);
        printf("Server> ");
        fflush(stdout);
    }
    
    pthread_mutex_unlock(&clients_mutex);
}

// Function to receive messages from clients
void *receive_messages(void *arg) {
    char buffer[BUFFER_SIZE];
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        
        // Receive data from client
        int recv_len = recvfrom(server_socket, buffer, BUFFER_SIZE - 1, 0,
                               (struct sockaddr *)&client_addr, &client_len);
        
        if (recv_len > 0) {
            buffer[recv_len] = '\0';
            
            // Add client if new
            add_client(client_addr);
            
            // Display received message
            printf("\n[%s:%d]: %s\n", 
                   inet_ntoa(client_addr.sin_addr),
                   ntohs(client_addr.sin_port),
                   buffer);
            printf("Server> ");
            fflush(stdout);
            
            // Check for disconnect message
            if (strcmp(buffer, "disconnect") == 0 || strcmp(buffer, "DISCONNECT") == 0) {
                printf("[CLIENT DISCONNECTED]\n");
                printf("Server> ");
                fflush(stdout);
            }
        }
    }
    
    return NULL;
}

// Function to send messages to clients
void *send_messages(void *arg) {
    char message[BUFFER_SIZE];
    
    while (1) {
        printf("Server> ");
        fflush(stdout);
        
        if (fgets(message, BUFFER_SIZE, stdin) == NULL) {
            break;
        }
        
        // Remove newline character
        message[strcspn(message, "\n")] = 0;
        
        // Check for quit command
        if (strcmp(message, "quit") == 0 || strcmp(message, "QUIT") == 0) {
            printf("Shutting down server...\n");
            close(server_socket);
            exit(0);
        }
        
        // List connected clients
        if (strcmp(message, "list") == 0 || strcmp(message, "LIST") == 0) {
            pthread_mutex_lock(&clients_mutex);
            printf("\n--- Connected Clients ---\n");
            if (client_count == 0) {
                printf("No clients connected\n");
            } else {
                for (int i = 0; i < client_count; i++) {
                    printf("%d. %s:%d\n", i + 1,
                           inet_ntoa(clients[i].address.sin_addr),
                           ntohs(clients[i].address.sin_port));
                }
            }
            pthread_mutex_unlock(&clients_mutex);
            continue;
        }
        
        // Send message to all connected clients
        pthread_mutex_lock(&clients_mutex);
        if (client_count == 0) {
            printf("No clients connected yet!\n");
        } else {
            for (int i = 0; i < client_count; i++) {
                if (sendto(server_socket, message, strlen(message), 0,
                          (struct sockaddr *)&clients[i].address,
                          sizeof(clients[i].address)) < 0) {
                    perror("Send failed");
                }
            }
        }
        pthread_mutex_unlock(&clients_mutex);
    }
    
    return NULL;
}

int main() {
    struct sockaddr_in server_addr;
    pthread_t recv_thread, send_thread;
    
    // Create UDP socket
    server_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_socket < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    
    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    
    // Bind socket to address
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }
    
    printf("==============================================\n");
    printf("UDP Server started on port %d\n", PORT);
    printf("Waiting for clients to connect...\n");
    printf("Commands: 'list' - show clients, 'quit' - exit\n");
    printf("==============================================\n\n");
    
    // Create threads for receiving and sending
    if (pthread_create(&recv_thread, NULL, receive_messages, NULL) != 0) {
        perror("Failed to create receive thread");
        close(server_socket);
        exit(EXIT_FAILURE);
    }
    
    if (pthread_create(&send_thread, NULL, send_messages, NULL) != 0) {
        perror("Failed to create send thread");
        close(server_socket);
        exit(EXIT_FAILURE);
    }
    
    // Wait for threads to finish
    pthread_join(recv_thread, NULL);
    pthread_join(send_thread, NULL);
    
    close(server_socket);
    return 0;
}