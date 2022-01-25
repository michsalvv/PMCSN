#include <stdio.h>
#include <stdlib.h>

#include "DES/rngs.h"
#include "DES/rvgs.h"
#include "config.h"
#include "math.h"
#include "structures.h"
#include "utils.h"

int streamID;

int main() {
    int s, e;
    long number = 0;  // number = #job in coda
    init_system();

    clock.current = START;
    clock.arrival = getArrival(clock.current);

    for (int i = 1; i < TEMP_SERVERS; i++) {
        server *s = (server *)malloc(sizeof(server));
        s->id = i + 1;
        s->status = 0;
        s->nodeType = TEMPERATURE_CTRL;
        s->completion = INFINITY;
        s->stream = streamID++;
        server *last = iterateOver(blocks[0].firstServer);
        last->next = s;
    }

    server *head_2 = (server *)malloc(sizeof(server));
    head_2->id = 1;
    head_2->nodeType = TICKETS_BUY;
    head_2->completion = INFINITY;
    head_2->stream = streamID++;
    blocks[1].firstServer = head_2;

    // Init ticket server
    for (int i = 1; i < TICKET_SERVERS; i++) {
        server *s = (server *)malloc(sizeof(server));
        s->id = i + 1;
        s->status = 0;
        s->nodeType = TICKETS_BUY;
        s->completion = INFINITY;
        s->stream = streamID++;
        iterateOver(blocks[1].firstServer)->next = s;
    }

    printServerList(blocks[0].firstServer);
    printServerList(blocks[1].firstServer);

    server *server_completion;
    while (clock.arrival <= STOP) {
        clock.next = findNextEvent(clock.arrival, blocks, server_completion);  // Prossimo evento
        clock.current = clock.next;                                            // avanzamento clock

        // Gestione arrivo dall'esterno, quindi in TEMPERATURE_CTRL
        if (clock.current == clock.arrival) {
            process_arrival();
        }

        // Gestione Completamento
        else {
            process_completion(server_completion);
        }
        printServerList(blocks[0].firstServer);
    }
}

// Inizializza tutti i blocchi del sistema
void init_system() {
    streamID = 0;
    blocks[0].num_server = TEMP_SERVERS;
    blocks[1].num_server = TICKET_SERVERS;

    // Inizializza il blocco per il controllo della temperatura
    server *head_1 = (server *)malloc(sizeof(server));
    head_1->id = 1;
    head_1->nodeType = TEMPERATURE_CTRL;
    head_1->completion = INFINITY;
    head_1->stream = streamID++;
    blocks[0].firstServer = head_1;
}

/*
* Genera un tempo di Arrivo secondo la distribuzione specificata
* TODO gestire le fascie orarie
*/
double getArrival(double current) {
    double arrival = current;
    SelectStream(254);
    arrival += Exponential(0.346153833);
    return (arrival);
}

// Inserisce un job nella coda del blocco specificata
void enqueue(struct node block, double arrival) {
    struct job *j = (struct job *)malloc(sizeof(struct job));
    if (j == NULL)
        handle_error("malloc");

    j->arrival = arrival;
    j->next = NULL;

    if (block.tail)  // Appendi alla coda se esiste, altrimenti è la testa
        block.tail->next = j;
    else
        block.head = j;

    block.tail = j;
}

// Ritorna e rimuove il job dalla coda del blocco specificata
struct job dequeue(struct node block) {
    struct job *j = block.head;

    if (!j->next)
        block.tail = NULL;

    block.head = j->next;
    free(j);
}

// Ritorna un server libero nella Linked List del blocco
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

// Genera un tempo di servizio secondo la distribuzione specificata
double getService(enum node_type type, int id) {
    SelectStream(id);
    double x;

    switch (type) {
        case TEMPERATURE_CTRL:
            return Exponential(0.5);
        default:
            return 0;
    }
}

/*
Processa un arrivo dall'esterno
*/
void process_arrival() {
    server *s = findFreeServer(blocks[0].firstServer);

    // C'è un servente libero
    if (s != NULL) {
        double serviceTime = getService(TEMPERATURE_CTRL, s->stream);
        s->completion = clock.current + serviceTime;
        s->status = 1;  // Setto stato busy
    }
    enqueue(blocks[0], clock.arrival);  // lo appendo nella coda del blocco TEMP

    clock.arrival = getArrival(clock.current);  // Genera prossimo arrivo
}

/*
If completamento
vedo servizio relativo al completamento
tolgo dalla coda (tolgo la testa) 
Determino la sua prossima destinazione e lo metto nella coda destinazione con tempo di arrivo = tempo di completamento
generi un tempo di servizio per il job in testa alla coda
*/
void process_completion(server * compl ) {
    switch (compl ->nodeType) {
        case TEMPERATURE_CTRL:
            struct job j = dequeue(blocks[0]);
            enum node_type destination = getDestination(compl ->nodeType);
            enqueue(blocks[destination], compl ->completion);  // Lo rimetto nella coda del blocco destinazione e gli imposto come tempo di arrivo quello di completamento

            // Se il blocco in cui si è verificato il completamento ha altri job in coda
            // allora bisogna generare il prossimo tempo di completamento per il server che si è liberato
            // if job in coda nel blocco > 0
            if (1)
                compl ->completion = getService(TICKETS_BUY, compl ->stream);

            break;

        default:
            break;
    }
}