#include <stdio.h>
#include <stdlib.h>

#include "DES/rngs.h"
#include "DES/rvgs.h"
#include "math.h"

#define START 0.0
#define STOP 18000.0

enum node_type { TEMP,
                 TICKETS };
struct job {
    double arrival;
    struct job *next;
};

struct server_t {
    int id;
    int status;  // {0=idle, 1=busy}
    struct server_t *next;
};
typedef struct server_t server;

struct node {
    struct job *head;
    struct job *tail;
    struct job in_service;
    struct job *head_second;
    struct job *tail_second;
    // struct area area;
    // double opening_time;
    int stream;
    double active_time;
    int number;
    double completion;
    enum node_type type;  // forse non serve

    int num_server;
    server *firstServer;

} servers[2];

double getFirstArrival() {
    static double arrival = START;
    SelectStream(254);
    arrival += Exponential(0.346153833);
    return (arrival);
}
	
server *iterateOver(server s) {
    printf("DIO");
    server current = s;
    while (&current != NULL) {
        current = current.next;
    }
    return current;
}

int main() {
    int s, e;
    long number = 0;  // number = #job in coda

    // Clock Struct
    struct {
        double current;  /* current time                       */
        double next;     /* next (most imminent) event time    */
        double arrival;  // next arrival
    } clock;

    clock.current = START;

    int streamID = 0;
    servers[0].num_server = 3;
    servers[1].num_server = 4;

    server *head_1 = (server *)malloc(sizeof(server));
    head_1->id = 1;
    servers[0].firstServer = head_1;
    server *head_2 = (server *)malloc(sizeof(server));
    head_2->id = 1;
    servers[1].firstServer = head_2;

    for (int i = 1; i < 3; i++) {
        server *s = (server *)malloc(sizeof(server));
        s->id = i + 1;
		printf("TEST\n");
		server *last = iterateOver(servers[0].firstServer);
		last->next = s;
        printf("TEST\n");
    }

    for (int i = 1; i < 4; i++) {
        server *s = (server *)malloc(sizeof(server));
        s->id = i + 1;
        iterateOver(servers[1].firstServer)->next = s;
    }

    server *current = servers[0].firstServer;
    while (current != NULL) {
        if (current->next == NULL)
            printf("%d\n", current->id);
        else
            printf("%d -> %d", current->id, current->next->id);
    }
}