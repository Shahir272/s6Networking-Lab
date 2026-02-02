#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>
#include <pthread.h>

#define PORT 9090
#define BUFFER_SIZE 1024

typedef struct {
    struct sockaddr_in client_addr;
    socklen_t client_len;
    char request[BUFFER_SIZE];
    int server_fd;
} ClientRequest;

void get_system_time(char *time_str, size_t size) {
    time_t current_time;
    struct tm *time_info;
    
    time(&current_time);
    time_info = localtime(&current_time);
    strftime(time_str, size, "%Y-%m-%d %H:%M:%S %Z", time_info);
}

void *handle_client_request(void *arg) {
    ClientRequest *req = (ClientRequest *)arg;
    char time_str[BUFFER_SIZE];
    
    printf("[REQUEST] From %s:%d - %s\n",
           inet_ntoa(req->client_addr.sin_addr),
           ntohs(req->client_addr.sin_port),
           req->request);
    
    get_system_time(time_str, sizeof(time_str));
    
    sendto(req->server_fd, time_str, strlen(time_str), 0,
           (struct sockaddr *)&req->client_addr, req->client_len);
    
    printf("[RESPONSE] Sent to %s:%d - %s\n",
           inet_ntoa(req->client_addr.sin_addr),
           ntohs(req->client_addr.sin_port),
           time_str);
    
    free(req);
    pthread_exit(NULL);
}

int main() {
    int server_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    char buffer[BUFFER_SIZE];
    pthread_t thread_id;
    
    // Create UDP socket
    server_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_fd == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    printf("[SUCCESS] UDP Socket created\n");
    
    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    
    // Bind socket
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    printf("[SUCCESS] Bound to port %d\n", PORT);
    printf("[LISTENING] Time Server ready. Press Ctrl+C to stop\n\n");
    
    // Main loop - handle requests concurrently
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        client_len = sizeof(client_addr);
        
        // Receive request
        int bytes = recvfrom(server_fd, buffer, BUFFER_SIZE, 0,
                            (struct sockaddr *)&client_addr, &client_len);
        
        if (bytes < 0) {
            perror("Receive failed");
            continue;
        }
        
        // Create thread for this request
        ClientRequest *req = (ClientRequest *)malloc(sizeof(ClientRequest));
        if (req == NULL) continue;
        
        req->client_addr = client_addr;
        req->client_len = client_len;
        req->server_fd = server_fd;
        strncpy(req->request, buffer, BUFFER_SIZE - 1);
        
        if (pthread_create(&thread_id, NULL, handle_client_request, req) == 0) {
            pthread_detach(thread_id);
        } else {
            free(req);
        }
    }
    
    close(server_fd);
    return 0;
}