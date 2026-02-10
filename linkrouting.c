/*
 * Simple Link State Routing Algorithm in C
 * Uses Dijkstra's Algorithm for shortest path computation
 */

#include <stdio.h>
#include <limits.h>
#include <stdbool.h>

#define N 5           // Number of routers
#define INF 999       // Infinity (unreachable)

// Link state database: topology[i][j] = cost of direct link from i to j
int topology[N][N];

// Routing table: distance[i] = shortest distance from source
// next_hop[i] = next hop to reach router i
int distance[N];
int next_hop[N];
bool visited[N];

// Initialize the network topology
void init_topology() {
    // Initialize all links to infinity (no connection)
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            if (i == j) {
                topology[i][j] = 0;  // Distance to self is 0
            } else {
                topology[i][j] = INF;
            }
        }
    }
    
    // Define the network topology (bidirectional links)
    // Router 0 - Router 1: cost 2
    topology[0][1] = 2;
    topology[1][0] = 2;
    
    // Router 0 - Router 3: cost 6
    topology[0][3] = 6;
    topology[3][0] = 6;
    
    // Router 1 - Router 2: cost 3
    topology[1][2] = 3;
    topology[2][1] = 3;
    
    // Router 1 - Router 3: cost 8
    topology[1][3] = 8;
    topology[3][1] = 8;
    
    // Router 1 - Router 4: cost 5
    topology[1][4] = 5;
    topology[4][1] = 5;
    
    // Router 2 - Router 4: cost 7
    topology[2][4] = 7;
    topology[4][2] = 7;
    
    // Router 3 - Router 4: cost 9
    topology[3][4] = 9;
    topology[4][3] = 9;
}

// Print the network topology
void print_topology() {
    printf("\nNetwork Topology (Link State Database):\n");
    printf("========================================\n");
    printf("     ");
    for (int i = 0; i < N; i++) {
        printf("%4d", i);
    }
    printf("\n");
    
    for (int i = 0; i < N; i++) {
        printf("%4d:", i);
        for (int j = 0; j < N; j++) {
            if (topology[i][j] == INF) {
                printf(" INF");
            } else {
                printf("%4d", topology[i][j]);
            }
        }
        printf("\n");
    }
}

// Find the unvisited node with minimum distance
int find_min_distance() {
    int min = INF;
    int min_index = -1;
    
    for (int i = 0; i < N; i++) {
        if (!visited[i] && distance[i] < min) {
            min = distance[i];
            min_index = i;
        }
    }
    
    return min_index;
}

// Dijkstra's algorithm to compute shortest paths from source router
void dijkstra(int source) {
    // Initialize
    for (int i = 0; i < N; i++) {
        distance[i] = INF;
        next_hop[i] = -1;
        visited[i] = false;
    }
    
    distance[source] = 0;
    next_hop[source] = source;
    
    printf("\nRunning Dijkstra's Algorithm from Router %d:\n", source);
    printf("=============================================\n");
    
    // Find shortest path for all nodes
    for (int count = 0; count < N - 1; count++) {
        // Pick minimum distance vertex from unvisited vertices
        int u = find_min_distance();
        
        if (u == -1) break;  // No more reachable nodes
        
        visited[u] = true;
        printf("Step %d: Visiting router %d (distance = %d)\n", count + 1, u, distance[u]);
        
        // Update distance values of adjacent vertices
        for (int v = 0; v < N; v++) {
            if (!visited[v] && topology[u][v] != INF) {
                int new_distance = distance[u] + topology[u][v];
                
                if (new_distance < distance[v]) {
                    distance[v] = new_distance;
                    
                    // Set next hop
                    if (u == source) {
                        next_hop[v] = v;  // Direct neighbor
                    } else {
                        next_hop[v] = next_hop[u];  // Via same next hop as u
                    }
                    
                    printf("        Updated router %d: distance = %d, next_hop = %d\n", 
                           v, distance[v], next_hop[v]);
                }
            }
        }
    }
}

// Print routing table for a router
void print_routing_table(int source) {
    printf("\n========================================\n");
    printf("Routing Table for Router %d\n", source);
    printf("========================================\n");
    printf("Dest\tDistance\tNext Hop\tPath\n");
    printf("----\t--------\t--------\t----\n");
    
    for (int i = 0; i < N; i++) {
        printf("%d\t", i);
        
        if (distance[i] == INF) {
            printf("INF\t\t-\t\tUnreachable\n");
        } else {
            printf("%d\t\t", distance[i]);
            if (i == source) {
                printf("%d\t\t", i);
                printf("%d (self)\n", i);
            } else {
                printf("%d\t\t", next_hop[i]);
                
                // Show simple path indication
                if (next_hop[i] == i) {
                    printf("%d -> %d (direct)\n", source, i);
                } else {
                    printf("%d -> %d -> ... -> %d\n", source, next_hop[i], i);
                }
            }
        }
    }
}

// Simulate broadcasting Link State Advertisements
void broadcast_lsa(int router) {
    printf("\n*** Router %d broadcasts its Link State Advertisement ***\n", router);
    printf("Router %d is connected to:\n", router);
    
    for (int i = 0; i < N; i++) {
        if (i != router && topology[router][i] != INF) {
            printf("  - Router %d with cost %d\n", i, topology[router][i]);
        }
    }
}

int main() {
    printf("======================================\n");
    printf("LINK STATE ROUTING ALGORITHM\n");
    printf("======================================\n");
    
    // Initialize network
    init_topology();
    
    // Print topology
    print_topology();
    
    printf("\n\nNetwork Diagram:\n");
    printf("================\n");
    printf("\n");
    printf("        2---3---1---5---4\n");
    printf("        |       |\\     /|\n");
    printf("        |       | \\   / |\n");
    printf("        7       8  \\ /  9\n");
    printf("        |       |   X   |\n");
    printf("        |       |  / \\  |\n");
    printf("        |       | /   \\ |\n");
    printf("        2-------0---6---3\n");
    printf("\n");
    
    // Simulate LSA flooding
    printf("\n=== Phase 1: Link State Advertisement Flooding ===\n");
    for (int i = 0; i < N; i++) {
        broadcast_lsa(i);
    }
    
    printf("\n\n=== Phase 2: Each router builds complete topology map ===\n");
    printf("All routers now have the complete network topology!\n");
    
    // Compute routing tables for each router using Dijkstra's algorithm
    printf("\n\n=== Phase 3: Each router computes shortest paths ===\n");
    
    for (int i = 0; i < N; i++) {
        dijkstra(i);
        print_routing_table(i);
    }
    
    printf("\n\n========================================\n");
    printf("Link State Routing Complete!\n");
    printf("========================================\n");
    printf("\nKey Differences from Distance Vector:\n");
    printf("1. Each router knows the complete network topology\n");
    printf("2. Uses Dijkstra's algorithm (not Bellman-Ford)\n");
    printf("3. Faster convergence\n");
    printf("4. No count-to-infinity problem\n");
    printf("5. More memory required (stores entire topology)\n");
    
    return 0;
}