#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <stdbool.h>

#define PORT 9090
#define MAX_NODES 10
#define INFINITY_COST 9999
#define UPDATE_INTERVAL 5

// Routing table entry
typedef struct {
    int dest;           // Destination node
    int cost;           // Cost to reach destination
    int next_hop;       // Next hop to reach destination
} RouteEntry;

// Distance Vector packet
typedef struct {
    int node_id;                        // Sender node ID
    int num_entries;                    // Number of routing entries
    RouteEntry entries[MAX_NODES];      // Routing table entries
} DVPacket;

// Neighbor information
typedef struct {
    int node_id;
    char ip[16];
    int port;
    int cost;           // Direct link cost
    bool active;
} Neighbor;

// Global variables
int my_node_id;
RouteEntry routing_table[MAX_NODES];
int num_routes = 0;
Neighbor neighbors[MAX_NODES];
int num_neighbors = 0;
pthread_mutex_t table_mutex = PTHREAD_MUTEX_INITIALIZER;

// Function prototypes
void initialize_routing_table();
void print_routing_table();
void update_routing_table(DVPacket *packet, int neighbor_cost);
void send_distance_vector(int sock, struct sockaddr_in *dest_addr);
void *receive_updates(void *arg);
void *periodic_update(void *arg);
int find_route(int dest);
void add_neighbor(int node_id, char *ip, int port, int cost);

// Initialize routing table
void initialize_routing_table() {
    pthread_mutex_lock(&table_mutex);
    
    // Add route to self
    routing_table[0].dest = my_node_id;
    routing_table[0].cost = 0;
    routing_table[0].next_hop = my_node_id;
    num_routes = 1;
    
    // Add direct neighbors
    for (int i = 0; i < num_neighbors; i++) {
        routing_table[num_routes].dest = neighbors[i].node_id;
        routing_table[num_routes].cost = neighbors[i].cost;
        routing_table[num_routes].next_hop = neighbors[i].node_id;
        num_routes++;
    }
    
    pthread_mutex_unlock(&table_mutex);
    
    printf("[INIT] Routing table initialized\n");
    print_routing_table();
}

// Print routing table
void print_routing_table() {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║            ROUTING TABLE - NODE %d                        ║\n", my_node_id);
    printf("╠═══════════════════════════════════════════════════════════╣\n");
    printf("║ Destination │    Cost    │  Next Hop                      ║\n");
    printf("╠═════════════╪════════════╪════════════════════════════════╣\n");
    
    pthread_mutex_lock(&table_mutex);
    for (int i = 0; i < num_routes; i++) {
        if (routing_table[i].cost < INFINITY_COST) {
            printf("║     %2d      │    %4d    │     %2d                       ║\n",
                   routing_table[i].dest,
                   routing_table[i].cost,
                   routing_table[i].next_hop);
        }
    }
    pthread_mutex_unlock(&table_mutex);
    
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");
}

// Find route index for a destination
int find_route(int dest) {
    for (int i = 0; i < num_routes; i++) {
        if (routing_table[i].dest == dest) {
            return i;
        }
    }
    return -1;
}

// Update routing table based on received distance vector (Bellman-Ford)
void update_routing_table(DVPacket *packet, int neighbor_cost) {
    bool updated = false;
    
    pthread_mutex_lock(&table_mutex);
    
    printf("[UPDATE] Processing distance vector from Node %d\n", packet->node_id);
    
    for (int i = 0; i < packet->num_entries; i++) {
        int dest = packet->entries[i].dest;
        int received_cost = packet->entries[i].cost;
        
        // Skip if destination is self
        if (dest == my_node_id) continue;
        
        // Calculate new cost: cost to neighbor + neighbor's cost to destination
        int new_cost = neighbor_cost + received_cost;
        
        // Prevent routing loops and infinity
        if (new_cost >= INFINITY_COST) continue;
        
        int route_idx = find_route(dest);
        
        if (route_idx == -1) {
            // New destination - add to routing table
            if (num_routes < MAX_NODES) {
                routing_table[num_routes].dest = dest;
                routing_table[num_routes].cost = new_cost;
                routing_table[num_routes].next_hop = packet->node_id;
                num_routes++;
                updated = true;
                printf("  [NEW] Route to Node %d via Node %d (cost: %d)\n", 
                       dest, packet->node_id, new_cost);
            }
        } else {
            // Existing destination - check if new path is better
            if (new_cost < routing_table[route_idx].cost) {
                routing_table[route_idx].cost = new_cost;
                routing_table[route_idx].next_hop = packet->node_id;
                updated = true;
                printf("  [BETTER] Updated route to Node %d via Node %d (cost: %d)\n",
                       dest, packet->node_id, new_cost);
            }
        }
    }
    
    pthread_mutex_unlock(&table_mutex);
    
    if (updated) {
        printf("[UPDATE] ✓ Routing table updated\n");
        print_routing_table();
    } else {
        printf("[UPDATE] No changes to routing table\n\n");
    }
}

// Send distance vector to neighbors
void send_distance_vector(int sock, struct sockaddr_in *dest_addr) {
    DVPacket packet;
    
    pthread_mutex_lock(&table_mutex);
    
    packet.node_id = my_node_id;
    packet.num_entries = num_routes;
    
    for (int i = 0; i < num_routes; i++) {
        packet.entries[i] = routing_table[i];
    }
    
    pthread_mutex_unlock(&table_mutex);
    
    sendto(sock, &packet, sizeof(DVPacket), 0,
           (struct sockaddr *)dest_addr, sizeof(*dest_addr));
}

// Add neighbor
void add_neighbor(int node_id, char *ip, int port, int cost) {
    if (num_neighbors < MAX_NODES) {
        neighbors[num_neighbors].node_id = node_id;
        strcpy(neighbors[num_neighbors].ip, ip);
        neighbors[num_neighbors].port = port;
        neighbors[num_neighbors].cost = cost;
        neighbors[num_neighbors].active = true;
        num_neighbors++;
        printf("[CONFIG] Added neighbor: Node %d (%s:%d) with cost %d\n",
               node_id, ip, port, cost);
    }
}

// Thread to receive distance vector updates
void *receive_updates(void *arg) {
    int sock = *(int *)arg;
    DVPacket packet;
    struct sockaddr_in sender_addr;
    socklen_t addr_len = sizeof(sender_addr);
    
    printf("[THREAD] Distance Vector receiver started\n");
    
    while (1) {
        int recv_len = recvfrom(sock, &packet, sizeof(DVPacket), 0,
                               (struct sockaddr *)&sender_addr, &addr_len);
        
        if (recv_len > 0) {
            printf("\n[RECV] Distance vector received from Node %d (%s)\n",
                   packet.node_id, inet_ntoa(sender_addr.sin_addr));
            
            // Find neighbor's cost
            int neighbor_cost = INFINITY_COST;
            for (int i = 0; i < num_neighbors; i++) {
                if (neighbors[i].node_id == packet.node_id) {
                    neighbor_cost = neighbors[i].cost;
                    break;
                }
            }
            
            // Update routing table using Bellman-Ford algorithm
            update_routing_table(&packet, neighbor_cost);
        }
    }
    
    return NULL;
}

// Thread to periodically send updates
void *periodic_update(void *arg) {
    int sock = *(int *)arg;
    struct sockaddr_in neighbor_addr;
    
    printf("[THREAD] Periodic update sender started (interval: %d seconds)\n",
           UPDATE_INTERVAL);
    
    sleep(2); // Initial delay
    
    while (1) {
        printf("\n[PERIODIC] Sending distance vectors to all neighbors...\n");
        
        for (int i = 0; i < num_neighbors; i++) {
            if (neighbors[i].active) {
                memset(&neighbor_addr, 0, sizeof(neighbor_addr));
                neighbor_addr.sin_family = AF_INET;
                neighbor_addr.sin_port = htons(neighbors[i].port);
                inet_pton(AF_INET, neighbors[i].ip, &neighbor_addr.sin_addr);
                
                send_distance_vector(sock, &neighbor_addr);
                printf("  → Sent to Node %d (%s:%d)\n",
                       neighbors[i].node_id, neighbors[i].ip, neighbors[i].port);
            }
        }
        
        printf("[PERIODIC] Update cycle complete\n\n");
        sleep(UPDATE_INTERVAL);
    }
    
    return NULL;
}

int main(int argc, char *argv[]) {
    int sock;
    struct sockaddr_in server_addr;
    pthread_t recv_thread, update_thread;
    
    if (argc < 2) {
        printf("Usage: %s <node_id>\n", argv[0]);
        printf("Example: %s 1\n", argv[0]);
        return 1;
    }
    
    my_node_id = atoi(argv[1]);
    
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║     DISTANCE VECTOR ROUTING - NODE %d                     ║\n", my_node_id);
    printf("║     Protocol: Bellman-Ford Algorithm                      ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");
    
    // Create UDP socket
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        return 1;
    }
    
    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT + my_node_id);
    
    // Bind socket
    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(sock);
        return 1;
    }
    
    printf("[SERVER] Listening on port %d\n\n", PORT + my_node_id);
    
    // Configure network topology
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║                NETWORK CONFIGURATION                      ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");
    
    // Example topology - customize based on your network
    // Node 1 configuration
    if (my_node_id == 1) {
        add_neighbor(2, "127.0.0.1", PORT + 2, 1);
        add_neighbor(3, "127.0.0.1", PORT + 3, 4);
    }
    // Node 2 configuration
    else if (my_node_id == 2) {
        add_neighbor(1, "127.0.0.1", PORT + 1, 1);
        add_neighbor(3, "127.0.0.1", PORT + 3, 2);
        add_neighbor(4, "127.0.0.1", PORT + 4, 3);
    }
    // Node 3 configuration
    else if (my_node_id == 3) {
        add_neighbor(1, "127.0.0.1", PORT + 1, 4);
        add_neighbor(2, "127.0.0.1", PORT + 2, 2);
        add_neighbor(4, "127.0.0.1", PORT + 4, 1);
    }
    // Node 4 configuration
    else if (my_node_id == 4) {
        add_neighbor(2, "127.0.0.1", PORT + 2, 3);
        add_neighbor(3, "127.0.0.1", PORT + 3, 1);
    }
    
    printf("\n");
    
    // Initialize routing table
    initialize_routing_table();
    
    // Create threads
    if (pthread_create(&recv_thread, NULL, receive_updates, &sock) != 0) {
        perror("Failed to create receive thread");
        close(sock);
        return 1;
    }
    
    if (pthread_create(&update_thread, NULL, periodic_update, &sock) != 0) {
        perror("Failed to create update thread");
        close(sock);
        return 1;
    }
    
    printf("[MAIN] All systems operational\n");
    printf("[MAIN] Press Ctrl+C to stop\n\n");
    
    // Wait for threads
    pthread_join(recv_thread, NULL);
    pthread_join(update_thread, NULL);
    
    close(sock);
    return 0;
}