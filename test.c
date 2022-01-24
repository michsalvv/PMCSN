#include <stdio.h>
#include <stdlib.h>

#include "DES/rngs.h"
#include "DES/rvgs.h"
#include "config.h"
#include "math.h"

struct job {
    double arrival;
    struct job *next;
};

struct server_t {
    int id;
    int status;  // {0=idle, 1=busy}
    double completion;
    enum node_type nodeType;
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
    enum node_type type;  // forse non serve

    int num_server;
    server *firstServer;

} services[2];

/*
* Arrival on Temperature services
* TODO gestire le fascie orarie
*/
double getArrival(double current) {
    double arrival = current;
    SelectStream(254);
    arrival += Exponential(0.346153833);
    return (arrival);
}
	
server *iterateOver(server *s) {
    server *current = s;
    while (current != NULL) {
       if (current->next != NULL){
           current = current->next;
       }else break;
    }
    return current;
}

void printServerList(server *s){
    server *current = s;
    
    while (current != NULL)
    {
        printf("Server #%d \tStatus: %d\tCompletion: %f\n", current->id, current->status, current->completion);
        if (current->next == NULL) break;
        current = current->next;
    }
}

void enqueue(struct node service, server s, double arrival)
{
	struct job *j = (struct job *)malloc(sizeof(struct job));
	if (j == NULL)
		handle_error("malloc");

	j->arrival = arrival;
	j->next = NULL;
	
	if (service.tail)           // Appendi alla coda se esiste, altrimenti è la testa
		service.tail->next = j;
	else
		service.head = j;

	service.tail = j;
}

server* findFreeServer(struct node service){
    return service.firstServer;
}

double min(double x, double y){
    return (x<y) ?x :y;
}

/*
* Scorre tra i server di tutti i servizi per trovare l'imminente COMPLETAMENTO
*/
double nextCompletion(struct node *services){
   	double minCompletion = services[0].firstServer->completion;
	int numServices = NUM_SERVICES;
	for (int j = 0; j < numServices-1; j++) {
        server *current = services[j].firstServer;
        while (current != NULL) {
            minCompletion = min(minCompletion, current->completion);
        if (current->next != NULL){
            current = current->next;
        }else break;
        }
	}
	return minCompletion;
}

/*
*  Restituisce il clock relativo all'imminente evento (arrivo o completamento)
*/
double findNextEvent(double nextArrival, struct node *services){
    double completion = nextCompletion(services);
    return min(nextArrival, completion);
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
    clock.arrival = getArrival(clock.current);

    // init nodes
    int streamID = 0;
    services[0].num_server = TEMP_SERVERS;
    services[1].num_server = TICKET_SERVERS;
;

    server *head_1 = (server *)malloc(sizeof(server));
    head_1->id = 1;
    head_1->nodeType = TEMP;
    head_1->completion = INFINITY;
    services[0].firstServer = head_1;
    server *head_2 = (server *)malloc(sizeof(server));
    head_2->id = 1;
    head_2->nodeType = TICKETS;
    head_2->completion = INFINITY;
    services[1].firstServer = head_2;

    // Init temp server
    for (int i = 1; i < TEMP_SERVERS; i++) {
        server *s = (server *)malloc(sizeof(server));
        s->id = i + 1;
        s->status = 0;
        s->nodeType = TEMP;
        s->completion = INFINITY;
		server *last = iterateOver(services[0].firstServer);
		last->next = s;
    }
    // Init ticket server
    for (int i = 1; i < TICKET_SERVERS; i++) {
        server *s = (server *)malloc(sizeof(server));
        s->id = i + 1;
        s->status = 0;
        s->nodeType = TICKETS;
        s->completion = INFINITY;
        iterateOver(services[1].firstServer)->next = s;
    }

   printServerList(services[0].firstServer);
   printServerList(services[1].firstServer);

   while (clock.arrival <= STOP){
       
       clock.next = findNextEvent(clock.arrival, services);
       clock.current = clock.next;                               // avanzamento clock

        // Gestione arrivo in TEMP
       if (clock.current == clock.arrival){
           // IF ci sono serventi liberi lo metto in coda e gli genero un tempo di servizio
           server s = *findFreeServer(services[0]);
           // if (s != null) c'è un servente libero
           // generi un tempo di servizio per quel job e lo metti in coda
           enqueue(services[0], s, clock.arrival);

           clock.arrival = getArrival(clock.current);           // Genera prossimo arrivo
       }
       
       /*
       If completamento
       vedo servizio relativo al completamento
       tolgo dalla coda e lo metto nella coda destinazione con tempo di arrivo = tempo di completamento

       generi un tempo di servizio per il job in testa alla coda
       */
       printServerList(services[0].firstServer);
   }
    
}