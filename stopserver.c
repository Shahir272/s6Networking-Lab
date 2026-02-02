#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <errno.h>
#include <stdbool.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define TIMEOUT_SEC 2
#define MAX_RETRIES 5

// Frame structure
typedef struct {
    int seq_num;           // Sequence number (0 or 1)
    char data[BUFFER_SIZE]; // Data payload
    int data_len;          // Length of actual data
    int checksum;          // Checksum for error detection
} Frame;

// Acknowledgment structure
typedef struct {
    int ack_num;   // Acknowledgment number
    int checksum;  // Checksum
} Ack;

// Function prototypes
int calculate_checksum(char *data, int length);
bool verify_frame_checksum(Frame *frame);
void print_frame(Frame *frame, const char *prefix);
void print_ack(Ack *ack, const char *prefix);

// Calculate checksum (sum of bytes modulo 256)
int calculate_checksum(char *data, int length) {
    int sum = 0;
    for (int i = 0; i < length; i++) {
        sum += (unsigned char)data[i];
    }
    return sum % 256;
}

// Verify frame checksum
bool verify_frame_checksum(Frame *frame) {
    int calculated = calculate_checksum(frame->data, frame->data_len);
    return calculated == frame->checksum;
}

// Print frame details
void print_frame(Frame *frame, const char *prefix) {
    printf("  [%s FRAME] Seq: %d, DataLen: %d, Data: \"%.50s%s\", Checksum: %d\n", 
           prefix, 
           frame->seq_num, 
           frame->data_len,
           frame->data,
           frame->data_len > 50 ? "..." : "",
           frame->checksum);
}

// Print ACK details
void print_ack(Ack *ack, const char *prefix) {
    printf("  [%s ACK] Ack: %d, Checksum: %d\n", 
           prefix, ack->ack_num, ack->checksum);
}

int main() {
    int server_fd, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║     STOP-AND-WAIT ARQ SERVER (RECEIVER)                   ║\n");
    printf("║     Port: %d                                              ║\n", PORT);
    printf("╚════════════════════════════════════════════════════════════╝\n\n");
    
    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    
    // Set socket options to reuse address
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("Setsockopt failed");
        exit(EXIT_FAILURE);
    }
    
    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    
    // Bind socket
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }
    
    // Listen for connections
    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }
    
    printf("[SERVER] Listening on port %d...\n", PORT);
    printf("[SERVER] Waiting for client connection...\n\n");
    
    // Accept client connection
    if ((client_socket = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len)) < 0) {
        perror("Accept failed");
        exit(EXIT_FAILURE);
    }
    
    printf("[SERVER] Client connected from %s:%d\n", 
           inet_ntoa(client_addr.sin_addr), 
           ntohs(client_addr.sin_port));
    printf("[SERVER] Starting data reception...\n\n");
    
    int expected_seq = 0;  // Expecting sequence number 0 first
    int frame_count = 0;
    Frame frame;
    Ack ack;
    
    while (1) {
        // Receive frame
        int bytes_received = recv(client_socket, &frame, sizeof(Frame), 0);
        
        if (bytes_received <= 0) {
            if (bytes_received == 0) {
                printf("\n[SERVER] Client disconnected\n");
            } else {
                perror("[SERVER] Receive error");
            }
            break;
        }
        
        printf("\n[SERVER] Frame received (#%d)\n", frame_count + 1);
        print_frame(&frame, "RECV");
        
        // Check if this is the end signal (empty data)
        if (frame.data_len == 0) {
            printf("[SERVER] End of transmission signal received\n");
            
            // Send final ACK
            ack.ack_num = frame.seq_num;
            ack.checksum = frame.seq_num;
            send(client_socket, &ack, sizeof(Ack), 0);
            print_ack(&ack, "SEND");
            
            printf("\n[SERVER] All frames received successfully!\n");
            break;
        }
        
        // Verify checksum
        if (!verify_frame_checksum(&frame)) {
            printf("[SERVER] ❌ Checksum verification FAILED! Frame corrupted.\n");
            printf("[SERVER] Discarding frame. Waiting for retransmission...\n");
            
            // Don't send ACK for corrupted frame (sender will timeout and retransmit)
            continue;
        }
        
        printf("[SERVER] ✓ Checksum verified\n");
        
        // Check sequence number
        if (frame.seq_num == expected_seq) {
            printf("[SERVER] ✓ Correct sequence number (expected %d, got %d)\n", 
                   expected_seq, frame.seq_num);
            printf("[SERVER] Data: %s\n", frame.data);
            
            // Prepare and send ACK
            ack.ack_num = expected_seq;
            ack.checksum = expected_seq;  // Simple checksum
            
            if (send(client_socket, &ack, sizeof(Ack), 0) < 0) {
                perror("[SERVER] Failed to send ACK");
                break;
            }
            
            print_ack(&ack, "SEND");
            printf("[SERVER] ACK sent for frame %d\n", frame_count);
            
            // Move to next expected sequence
            frame_count++;
            expected_seq = 1 - expected_seq;  // Toggle between 0 and 1
        } else {
            printf("[SERVER] ⚠ Unexpected sequence number (expected %d, got %d)\n", 
                   expected_seq, frame.seq_num);
            printf("[SERVER] Duplicate or out-of-order frame detected\n");
            
            // Send ACK for the last correctly received frame
            ack.ack_num = 1 - expected_seq;  // ACK for previous frame
            ack.checksum = 1 - expected_seq;
            
            if (send(client_socket, &ack, sizeof(Ack), 0) < 0) {
                perror("[SERVER] Failed to send ACK");
                break;
            }
            
            print_ack(&ack, "SEND");
            printf("[SERVER] Re-sent ACK for previous frame\n");
        }
    }
    
    // Close sockets
    close(client_socket);
    close(server_fd);
    
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║              RECEPTION SUMMARY                             ║\n");
    printf("╠════════════════════════════════════════════════════════════╣\n");
    printf("║ Total frames successfully received: %-3d                  ║\n", frame_count);
    printf("║ Protocol: Stop-and-Wait ARQ                                ║\n");
    printf("║ Status: Transmission completed                             ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n\n");
    
    return 0;
}