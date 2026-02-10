/*
 * Simple Distance Vector Routing Algorithm in C
 */

#include <stdio.h>
#include <limits.h>

#define N 4           // Number of routers
#define INF 999       // Infinity (unreachable)

// Distance table: distance[i][j] = cost from router i to router j
int distance[N][N];

// Routing table: next_hop[i][j] = next hop from router i to reach router j
int next_hop[N][N];

// Initialize the network with direct link costs
void init_network() {
    // Initialize all distances to infinity
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            if (i == j) {
                distance[i][j] = 0;  // Distance to self is 0
                next_hop[i][j] = i;
            } else {
                distance[i][j] = INF;
                next_hop[i][j] = -1;
            }
        }
    }
    
    // Add direct links (cost between neighbors)
    // Router 0 connected to Router 1 with cost 1
    distance[0][1] = 1;
    distance[1][0] = 1;
    next_hop[0][1] = 1;
    next_hop[1][0] = 0;
    
    // Router 1 connected to Router 2 with cost 3
    distance[1][2] = 3;
    distance[2][1] = 3;
    next_hop[1][2] = 2;
    next_hop[2][1] = 1;
    
    // Router 2 connected to Router 3 with cost 1
    distance[2][3] = 1;
    distance[3][2] = 1;
    next_hop[2][3] = 3;
    next_hop[3][2] = 2;
    
    // Router 0 connected to Router 3 with cost 7
    distance[0][3] = 7;
    distance[3][0] = 7;
    next_hop[0][3] = 3;
    next_hop[3][0] = 0;
}

// Update distance vectors using Bellman-Ford algorithm
int update_routes() {
    int updated = 0;
    
    for (int i = 0; i < N; i++) {           // For each router
        for (int j = 0; j < N; j++) {       // For each destination
            for (int k = 0; k < N; k++) {   // Via each neighbor
                if (distance[i][k] != INF && distance[k][j] != INF) {
                    int new_cost = distance[i][k] + distance[k][j];
                    
                    if (new_cost < distance[i][j]) {
                        distance[i][j] = new_cost;
                        next_hop[i][j] = next_hop[i][k];
                        updated = 1;
                    }
                }
            }
        }
    }
    
    return updated;
}

// Print routing table for all routers
void print_routing_table() {
    printf("\nRouting Tables:\n");
    printf("===============\n");
    
    for (int i = 0; i < N; i++) {
        printf("\nRouter %d:\n", i);
        printf("Dest\tCost\tNext Hop\n");
        printf("----\t----\t--------\n");
        
        for (int j = 0; j < N; j++) {
            if (distance[i][j] == INF) {
                printf("%d\tINF\t-\n", j);
            } else {
                printf("%d\t%d\t%d\n", j, distance[i][j], next_hop[i][j]);
            }
        }
    }
}

int main() {
    printf("Distance Vector Routing Algorithm\n");
    printf("==================================\n");
    printf("\nNetwork Topology:\n");
    printf("  0 ---1--- 1 ---3--- 2 ---1--- 3\n");
    printf("  |                             |\n");
    printf("  +-----------7-----------------+\n");
    
    // Initialize network
    init_network();
    
    printf("\nInitial routing tables:\n");
    print_routing_table();
    
    // Run distance vector algorithm
    printf("\n\nRunning Distance Vector Algorithm...\n");
    
    int iteration = 0;
    while (update_routes() && iteration < 10) {
        iteration++;
        printf("\nAfter iteration %d:\n", iteration);
        print_routing_table();
    }
    
    printf("\n\nConverged after %d iterations!\n", iteration);
    printf("\nFinal routing tables:\n");
    print_routing_table();
    
    return 0;
}