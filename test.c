#include <stdio.h>
#include <stdlib.h>

#include "DES/rngs.h"
#include "DES/rvgs.h"
#include "config.h"
#include "utils.h"
#include "math.h"

struct job {
    double arrival;
    struct job *next;
};

// Servente
struct server_t {
    int id;
    int status;  // {0=idle, 1=busy}
    double completion;
    int stream;
    enum node_type nodeType;
    struct server_t *next;
};
typedef struct server_t server;

// Blocco
struct node {
    struct job *head;
    struct job *tail;
    struct job in_service;
    struct job *head_second;
    struct job *tail_second;
    // struct area area;
    // double opening_time;
    double active_time;
    int number;
    enum node_type type;  // forse non serve

    int num_server;
    server *firstServer;

} blocks[2];

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
        if (current->next != NULL) {
            current = current->next;
        } else
            break;
    }
    return current;
}

void printServerList(server *s) {
    server *current = s;

    while (current != NULL) {
        printf("Server #%d \tStatus: %d\tCompletion: %f\n", current->id, current->status, current->completion);
        if (current->next == NULL) break;
        current = current->next;
    }
}

void enqueue(struct node service, double arrival) {
    struct job *j = (struct job *)malloc(sizeof(struct job));
    if (j == NULL)
        handle_error("malloc");

    j->arrival = arrival;
    j->next = NULL;

    if (service.tail)  // Appendi alla coda se esiste, altrimenti è la testa
        service.tail->next = j;
    else
        service.head = j;

    service.tail = j;
}

struct job dequeue(struct node block){
    struct job *j = block.head;

	if (!j->next)
		block.tail = NULL;

	block.head = j->next;	
	free(j);
}

server *findFreeServer(server *s) {
    server *current = s;
    while (current != NULL) {
        if (current->status == 0) return current;
        if (current->next != NULL) {
            current = current->next;
        } else
            break;
    }
    return NULL;

}

double min(double x, double y) {
    return (x < y) ? x : y;
}

/*
* Scorre tra i server di tutti i servizi per trovare l'imminente COMPLETAMENTO
* Restituisce inoltre il server interessato
*/
double nextCompletion(struct node *services, server *s) {
    double minCompletion = services[0].firstServer->completion;
    int numServices = NUM_SERVICES;
    server *current;
    for (int j = 0; j < numServices - 1; j++) {
        current = services[j].firstServer;
        while (current != NULL) {
            minCompletion = min(minCompletion, current->completion);
            if (current->next != NULL) {
                current = current->next;
            } else
                break;
        }
    }
    s = current;
    return minCompletion;
}

/*
*  Restituisce il clock relativo all'imminente evento (arrivo o completamento)
*/
double findNextEvent(double nextArrival, struct node *services, server *server_completion) {
    double completion = nextCompletion(services, server_completion);
    return min(nextArrival, completion);
}

double getService(enum node_type type, int id)
{
	SelectStream(id);
	double x;

	switch (type) {
	case TEMP:
		return Exponential(0.5);
	default:
		return 0;
	}
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
    blocks[0].num_server = TEMP_SERVERS;
    blocks[1].num_server = TICKET_SERVERS;

    // Init temp server
    server *head_1 = (server *)malloc(sizeof(server));
    head_1->id = 1;
    head_1->nodeType = TEMP;
    head_1->completion = INFINITY;
    head_1->stream = streamID++;
    blocks[0].firstServer = head_1;

    for (int i = 1; i < TEMP_SERVERS; i++) {
        server *s = (server *)malloc(sizeof(server));
        s->id = i + 1;
        s->status = 0;
        s->nodeType = TEMP;
        s->completion = INFINITY;
        s->stream = streamID++;
        server *last = iterateOver(blocks[0].firstServer);
        last->next = s;
    }

    server *head_2 = (server *)malloc(sizeof(server));
    head_2->id = 1;
    head_2->nodeType = TICKETS;
    head_2->completion = INFINITY;
    head_2->stream = streamID++;
    blocks[1].firstServer = head_2;

   
    // Init ticket server
    for (int i = 1; i < TICKET_SERVERS; i++) {
        server *s = (server *)malloc(sizeof(server));
        s->id = i + 1;
        s->status = 0;
        s->nodeType = TICKETS;
        s->completion = INFINITY;
        s->stream = streamID++;
        iterateOver(blocks[1].firstServer)->next = s;
    }

    printServerList(blocks[0].firstServer);
    printServerList(blocks[1].firstServer);

    server *server_completion;
    while (clock.arrival <= STOP) {
        clock.next = findNextEvent(clock.arrival, blocks, server_completion);        // Prossimo evento
        clock.current = clock.next;                                                  // avanzamento clock

        // Gestione arrivo in TEMP
        if (clock.current == clock.arrival) {
            server *s = findFreeServer(blocks[0].firstServer);

            // C'è un servente libero
            if (s != NULL){
                double serviceTime = getService(TEMP, s->stream);
                s->completion = clock.current + serviceTime;
                s->status = 1;                                      // Setto stato busy
            }                                             
            enqueue(blocks[0], clock.arrival);                  // lo appendo nella coda del blocco TEMP

            clock.arrival = getArrival(clock.current);          // Genera prossimo arrivo
        }
        
        // Gestione Completamento 
        else{
            switch (server_completion->nodeType)
            {
            case TEMP:
                struct job j = dequeue(blocks[0]);
                enum node_type destination = getDestination(server_completion->nodeType);
                enqueue(blocks[destination], server_completion->completion);            // Lo rimetto nella coda del blocco destinazione e gli imposto come tempo di arrivo quello di completamento

                // Se il blocco in cui si è verificato il completamento ha altri job in coda
                // allora bisogna generare il prossimo tempo di completamento per il server che si è liberato
                if (job in coda nel blocco >0)
                server_completion->completion = getService(TICKETS, server_completion->stream);

                break;
            
            default:
                break;
            }

        }

        /*
       If completamento
       vedo servizio relativo al completamento
       tolgo dalla coda (tolgo la testa) 
       Determino la sua prossima destinazione e lo metto nella coda destinazione con tempo di arrivo = tempo di completamento

       generi un tempo di servizio per il job in testa alla coda
       */
        printServerList(blocks[0].firstServer);
    }
}