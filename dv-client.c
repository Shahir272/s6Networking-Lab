#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 9090
#define MAX_NODES 10
#define INFINITY_COST 9999
#define TIMEOUT_SEC 3

// Routing table entry
typedef struct {
    int dest;
    int cost;
    int next_hop;
} RouteEntry;

// Distance Vector packet
typedef struct {
    int node_id;
    int num_entries;
    RouteEntry entries[MAX_NODES];
} DVPacket;

// Query packet
typedef struct {
    int query_type;     // 0 = get routing table, 1 = find path
    int source;
    int destination;
} QueryPacket;

// Function prototypes
void print_routing_table(DVPacket *packet);
void query_routing_table(int node_id);
void find_path(int source, int dest);
void print_menu();

// Print routing table
void print_routing_table(DVPacket *packet) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║         ROUTING TABLE - NODE %d                           ║\n", packet->node_id);
    printf("╠═══════════════════════════════════════════════════════════╣\n");
    printf("║ Destination │    Cost    │  Next Hop                      ║\n");
    printf("╠═════════════╪════════════╪════════════════════════════════╣\n");
    
    for (int i = 0; i < packet->num_entries; i++) {
        if (packet->entries[i].cost < INFINITY_COST) {
            printf("║     %2d      │    %4d    │     %2d                       ║\n",
                   packet->entries[i].dest,
                   packet->entries[i].cost,
                   packet->entries[i].next_hop);
        }
    }
    
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");
}

// Query routing table from a node
void query_routing_table(int node_id) {
    int sock;
    struct sockaddr_in router_addr;
    DVPacket packet;
    struct timeval timeout;
    
    printf("\n[QUERY] Requesting routing table from Node %d...\n", node_id);
    
    // Create UDP socket
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        return;
    }
    
    // Set timeout
    timeout.tv_sec = TIMEOUT_SEC;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    
    // Configure router address
    memset(&router_addr, 0, sizeof(router_addr));
    router_addr.sin_family = AF_INET;
    router_addr.sin_port = htons(PORT + node_id);
    router_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    // Send query (empty packet triggers routing table response)
    QueryPacket query;
    query.query_type = 0;
    query.source = 0;
    query.destination = 0;
    
    sendto(sock, &query, sizeof(QueryPacket), 0,
           (struct sockaddr *)&router_addr, sizeof(router_addr));
    
    // Receive response
    socklen_t addr_len = sizeof(router_addr);
    int recv_len = recvfrom(sock, &packet, sizeof(DVPacket), 0,
                           (struct sockaddr *)&router_addr, &addr_len);
    
    if (recv_len > 0) {
        printf("[QUERY] ✓ Received routing table from Node %d\n", node_id);
        print_routing_table(&packet);
    } else {
        printf("[QUERY] ✗ Timeout - Node %d not responding\n\n", node_id);
    }
    
    close(sock);
}

// Find and display path between two nodes
void find_path(int source, int dest) {
    int sock;
    struct sockaddr_in router_addr;
    DVPacket packet;
    struct timeval timeout;
    int path[MAX_NODES];
    int path_length = 0;
    int total_cost = 0;
    
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║              PATH FINDING                                 ║\n");
    printf("║   Source: Node %d  →  Destination: Node %d               ║\n", source, dest);
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");
    
    // Create socket
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        return;
    }
    
    // Set timeout
    timeout.tv_sec = TIMEOUT_SEC;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    
    // Query source node
    memset(&router_addr, 0, sizeof(router_addr));
    router_addr.sin_family = AF_INET;
    router_addr.sin_port = htons(PORT + source);
    router_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    QueryPacket query;
    query.query_type = 0;
    sendto(sock, &query, sizeof(QueryPacket), 0,
           (struct sockaddr *)&router_addr, sizeof(router_addr));
    
    socklen_t addr_len = sizeof(router_addr);
    int recv_len = recvfrom(sock, &packet, sizeof(DVPacket), 0,
                           (struct sockaddr *)&router_addr, &addr_len);
    
    if (recv_len <= 0) {
        printf("[PATH] ✗ Cannot reach Node %d\n\n", source);
        close(sock);
        return;
    }
    
    // Build path
    path[path_length++] = source;
    int current = source;
    
    while (current != dest && path_length < MAX_NODES) {
        // Find route to destination
        int next_hop = -1;
        int route_cost = INFINITY_COST;
        
        for (int i = 0; i < packet.num_entries; i++) {
            if (packet.entries[i].dest == dest) {
                next_hop = packet.entries[i].next_hop;
                route_cost = packet.entries[i].cost;
                break;
            }
        }
        
        if (next_hop == -1 || route_cost >= INFINITY_COST) {
            printf("[PATH] ✗ No route from Node %d to Node %d\n\n", source, dest);
            close(sock);
            return;
        }
        
        // If next hop is destination, we're done
        if (next_hop == dest) {
            path[path_length++] = dest;
            total_cost = route_cost;
            break;
        }
        
        // Otherwise, query the next hop
        path[path_length++] = next_hop;
        current = next_hop;
        
        // Query next node
        router_addr.sin_port = htons(PORT + current);
        sendto(sock, &query, sizeof(QueryPacket), 0,
               (struct sockaddr *)&router_addr, sizeof(router_addr));
        
        recv_len = recvfrom(sock, &packet, sizeof(DVPacket), 0,
                           (struct sockaddr *)&router_addr, &addr_len);
        
        if (recv_len <= 0) {
            printf("[PATH] ✗ Lost connection while tracing path\n\n");
            close(sock);
            return;
        }
        
        // Find cost from current to destination
        for (int i = 0; i < packet.num_entries; i++) {
            if (packet.entries[i].dest == dest) {
                total_cost = route_cost;
                break;
            }
        }
    }
    
    // Display path
    if (path[path_length - 1] == dest) {
        printf("[PATH] ✓ Route found!\n\n");
        printf("Path: ");
        for (int i = 0; i < path_length; i++) {
            printf("Node %d", path[i]);
            if (i < path_length - 1) {
                printf(" → ");
            }
        }
        printf("\n\n");
        printf("Total hops: %d\n", path_length - 1);
        printf("Total cost: %d\n\n", total_cost);
    } else {
        printf("[PATH] ✗ Path incomplete\n\n");
    }
    
    close(sock);
}

// Print menu
void print_menu() {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║       DISTANCE VECTOR ROUTING - CLIENT                    ║\n");
    printf("╠═══════════════════════════════════════════════════════════╣\n");
    printf("║  Commands:                                                ║\n");
    printf("║  1. query <node_id>          - View routing table         ║\n");
    printf("║  2. path <source> <dest>     - Find path between nodes    ║\n");
    printf("║  3. help                     - Show this menu             ║\n");
    printf("║  4. exit                     - Exit program               ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
}

int main() {
    char command[100];
    char cmd[20];
    int arg1, arg2;
    
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║     DISTANCE VECTOR ROUTING CLIENT                        ║\n");
    printf("║     Query routing information from network nodes          ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    
    print_menu();
    
    while (1) {
        printf("\nDVR-Client> ");
        fflush(stdout);
        
        if (fgets(command, sizeof(command), stdin) == NULL) {
            break;
        }
        
        // Remove newline
        command[strcspn(command, "\n")] = 0;
        
        // Parse command
        if (sscanf(command, "%s %d %d", cmd, &arg1, &arg2) >= 1) {
            if (strcmp(cmd, "query") == 0) {
                if (sscanf(command, "%s %d", cmd, &arg1) == 2) {
                    query_routing_table(arg1);
                } else {
                    printf("Usage: query <node_id>\n");
                }
            }
            else if (strcmp(cmd, "path") == 0) {
                if (sscanf(command, "%s %d %d", cmd, &arg1, &arg2) == 3) {
                    find_path(arg1, arg2);
                } else {
                    printf("Usage: path <source> <destination>\n");
                }
            }
            else if (strcmp(cmd, "help") == 0) {
                print_menu();
            }
            else if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "quit") == 0) {
                printf("\n[CLIENT] Goodbye!\n\n");
                break;
            }
            else {
                printf("Unknown command: %s\n", cmd);
                printf("Type 'help' for available commands\n");
            }
        }
    }
    
    return 0;
}