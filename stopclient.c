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
#define TIMEOUT_SEC 3
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
bool verify_ack_checksum(Ack *ack);
void print_frame(Frame *frame, const char *prefix);
void print_ack(Ack *ack, const char *prefix);
bool send_frame_with_retry(int socket, Frame *frame, int expected_ack);

// Calculate checksum (sum of bytes modulo 256)
int calculate_checksum(char *data, int length) {
    int sum = 0;
    for (int i = 0; i < length; i++) {
        sum += (unsigned char)data[i];
    }
    return sum % 256;
}

// Verify ACK checksum
bool verify_ack_checksum(Ack *ack) {
    return ack->ack_num == ack->checksum;
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

// Set socket timeout
bool set_socket_timeout(int socket, int seconds) {
    struct timeval timeout;
    timeout.tv_sec = seconds;
    timeout.tv_usec = 0;
    
    if (setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("Failed to set socket timeout");
        return false;
    }
    return true;
}

// Send frame and wait for ACK with retry mechanism
bool send_frame_with_retry(int socket, Frame *frame, int expected_ack) {
    Ack ack;
    int retry_count = 0;
    
    while (retry_count < MAX_RETRIES) {
        // Send frame
        if (send(socket, frame, sizeof(Frame), 0) < 0) {
            perror("[CLIENT] Failed to send frame");
            return false;
        }
        
        print_frame(frame, "SEND");
        printf("[CLIENT] Frame sent (Attempt %d/%d)\n", retry_count + 1, MAX_RETRIES);
        
        // Wait for ACK
        printf("[CLIENT] Waiting for ACK %d...\n", expected_ack);
        int bytes_received = recv(socket, &ack, sizeof(Ack), 0);
        
        if (bytes_received < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                printf("[CLIENT] ⏱ Timeout! No ACK received.\n");
                printf("[CLIENT] Retransmitting frame...\n\n");
                retry_count++;
                continue;
            } else {
                perror("[CLIENT] Receive error");
                return false;
            }
        }
        
        if (bytes_received == 0) {
            printf("[CLIENT] Server disconnected\n");
            return false;
        }
        
        // ACK received
        print_ack(&ack, "RECV");
        
        // Verify ACK checksum
        if (!verify_ack_checksum(&ack)) {
            printf("[CLIENT] ❌ ACK checksum verification FAILED!\n");
            printf("[CLIENT] Retransmitting frame...\n\n");
            retry_count++;
            continue;
        }
        
        printf("[CLIENT] ✓ ACK checksum verified\n");
        
        // Check if ACK matches expected sequence number
        if (ack.ack_num == expected_ack) {
            printf("[CLIENT] ✓ Correct ACK received (ACK %d)\n", expected_ack);
            printf("[CLIENT] Frame acknowledged successfully!\n");
            return true;
        } else {
            printf("[CLIENT] ⚠ Unexpected ACK %d (expected %d)\n", 
                   ack.ack_num, expected_ack);
            printf("[CLIENT] Retransmitting frame...\n\n");
            retry_count++;
        }
    }
    
    printf("[CLIENT] ❌ Max retries exceeded. Transmission failed.\n");
    return false;
}

int main(int argc, char *argv[]) {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char *server_ip = "127.0.0.1";  // Default to localhost
    
    // Allow custom server IP from command line
    if (argc > 1) {
        server_ip = argv[1];
    }
    
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║     STOP-AND-WAIT ARQ CLIENT (SENDER)                     ║\n");
    printf("║     Server: %s:%-5d                                  ║\n", server_ip, PORT);
    printf("╚════════════════════════════════════════════════════════════╝\n\n");
    
    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return -1;
    }
    
    // Set socket timeout
    if (!set_socket_timeout(sock, TIMEOUT_SEC)) {
        close(sock);
        return -1;
    }
    
    // Configure server address
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    
    // Convert IPv4 address from text to binary
    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
        perror("Invalid address / Address not supported");
        close(sock);
        return -1;
    }
    
    // Connect to server
    printf("[CLIENT] Connecting to server %s:%d...\n", server_ip, PORT);
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection failed");
        close(sock);
        return -1;
    }
    
    printf("[CLIENT] Connected to server!\n");
    printf("[CLIENT] Starting data transmission...\n\n");
    
    // Test data to send
    char *messages[] = {
        "Hello, this is message 1 from Stop-and-Wait ARQ client!",
        "Message 2: Testing reliable data transfer protocol.",
        "Message 3: Sequence numbers ensure proper ordering.",
        "Message 4: Checksums detect transmission errors.",
        "Message 5: Timeouts handle lost packets gracefully.",
        "Final message: All systems operational!"
    };
    
    int num_messages = sizeof(messages) / sizeof(messages[0]);
    int seq_num = 0;  // Start with sequence number 0
    int messages_sent = 0;
    
    // Send all messages
    for (int i = 0; i < num_messages; i++) {
        printf("\n═══════════════════════════════════════════════════════\n");
        printf("  Sending Message %d/%d (Seq: %d)\n", i + 1, num_messages, seq_num);
        printf("═══════════════════════════════════════════════════════\n\n");
        
        // Prepare frame
        Frame frame;
        frame.seq_num = seq_num;
        strncpy(frame.data, messages[i], BUFFER_SIZE - 1);
        frame.data[BUFFER_SIZE - 1] = '\0';  // Ensure null termination
        frame.data_len = strlen(frame.data);
        frame.checksum = calculate_checksum(frame.data, frame.data_len);
        
        // Send frame and wait for ACK
        if (!send_frame_with_retry(sock, &frame, seq_num)) {
            printf("\n[CLIENT] Failed to send message %d. Aborting.\n", i + 1);
            close(sock);
            return -1;
        }
        
        messages_sent++;
        seq_num = 1 - seq_num;  // Toggle between 0 and 1
        
        printf("\n[CLIENT] Message %d transmitted successfully!\n", i + 1);
        
        // Small delay between messages
        usleep(100000);  // 100ms
    }
    
    // Send end-of-transmission signal
    printf("\n═══════════════════════════════════════════════════════\n");
    printf("  Sending End-of-Transmission Signal\n");
    printf("═══════════════════════════════════════════════════════\n\n");
    
    Frame end_frame;
    end_frame.seq_num = seq_num;
    end_frame.data[0] = '\0';
    end_frame.data_len = 0;
    end_frame.checksum = 0;
    
    send_frame_with_retry(sock, &end_frame, seq_num);
    
    // Close socket
    close(sock);
    
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║              TRANSMISSION SUMMARY                          ║\n");
    printf("╠════════════════════════════════════════════════════════════╣\n");
    printf("║ Total messages sent: %-3d                                 ║\n", messages_sent);
    printf("║ Protocol: Stop-and-Wait ARQ                                ║\n");
    printf("║ Features Used:                                             ║\n");
    printf("║   • Sequence numbers (0, 1)                                ║\n");
    printf("║   • Checksum error detection                               ║\n");
    printf("║   • Timeout & retransmission                               ║\n");
    printf("║   • ACK verification                                       ║\n");
    printf("║ Status: Transmission completed successfully                ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n\n");
    
    return 0;
}