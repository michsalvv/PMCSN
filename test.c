#include <stdio.h>
#include <stdlib.h>

#include "DES/rngs.h"
#include "DES/rvgs.h"
#include "config.h"
#include "math.h"
#include "structures.h"
#include "utils.h"

double getArrival(double current);
void enqueue(struct node block, double arrival);
struct job dequeue(struct node block);
server *findFreeServer(server *s);
double nextCompletion(struct node *services, server *s);
double findNextEvent(double nextArrival, struct node *services, server *server_completion);
double getService(enum node_type type, int stream);
void process_arrival();
void process_completion(server * compl );
void init_network();
void init_blocks();

int streamID;               // Stream da selezionare per generare il tempo di servizio
server *server_completion;  // Tiene traccia del server relativo al completamento imminente

int main() {
    init_network();
    printServerList(blocks[0].firstServer);
    printServerList(blocks[1].firstServer);

    // Gestione degli arrivi e dei completamenti
    while (clock.arrival <= STOP) {
        clock.next = findNextEvent(clock.arrival, blocks, server_completion);  // Ottengo il prossimo evento
        clock.current = clock.next;                                            // Avanzamento del clock al valore del prossimo evento

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
    double minCompletion = services[TEMPERATURE_CTRL].firstServer->completion;
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

// Genera un tempo di servizio secondo la distribuzione specificata e stream del servente individuato
double getService(enum node_type type, int stream) {
    SelectStream(stream);
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
    server *s = findFreeServer(blocks[TEMPERATURE_CTRL].firstServer);

    // C'è un servente libero
    if (s != NULL) {
        double serviceTime = getService(TEMPERATURE_CTRL, s->stream);
        s->completion = clock.current + serviceTime;
        s->status = 1;  // Setto stato busy
    } else {
        blocks[TEMPERATURE_CTRL].jobInQueue++;  // Se non c'è un servente libero aumenta il numero di job in coda
    }
    enqueue(blocks[TEMPERATURE_CTRL], clock.arrival);  // lo appendo nella coda del blocco TEMP

    clock.arrival = getArrival(clock.current);  // Genera prossimo arrivo
}

void process_completion(server * compl ) {
    switch (compl ->nodeType) {
        case TEMPERATURE_CTRL:
            struct job j = dequeue(blocks[TEMPERATURE_CTRL]);  // Toglie il job servito dal blocco e fa "avanzare" la lista collegata di job

            // Se nel blocco temperatura ci sono job in coda, devo generare il prossimo completamento per il servente che si è liberato.
            if (blocks[TEMPERATURE_CTRL].jobInQueue > 0) {
                blocks[TEMPERATURE_CTRL].jobInQueue--;
                compl ->completion = clock.current + getService(TEMPERATURE_CTRL, compl ->stream);
            } else {
                compl ->completion = INFINITY;
                compl ->status = IDLE;
            }
            break;

            // Gestione blocco destinazione
            enum node_type destination = getDestination(compl ->nodeType);  // Trova la destinazione adatta per il job appena servito
            enqueue(blocks[destination], compl ->completion);               // Posiziono il job nella coda del blocco destinazione e gli imposto come tempo di arrivo quello di completamento

            // Se il blocco destinatario ha un servente libero, generiamo un tempo di completamento, altrimenti aumentiamo il numero di job in coda
            server *freeServer = findFreeServer(blocks[destination].firstServer);
            if (freeServer != NULL) {
                freeServer->completion = clock.current + getService(destination, freeServer->stream);
            } else {
                blocks[destination].jobInQueue++;
            }

        default:
            break;
    }
}

// Inizializza tutti i blocchi del sistema
void init_network() {
    streamID = 0;
    blocks[TEMPERATURE_CTRL].num_server = TEMPERATURE_CTRL_SERVERS;
    blocks[TICKET_BUY].num_server = TICKET_BUY_SERVERS;
    blocks[TICKET_GATE].num_server = TICKET_GATE_SERVERS;
    blocks[SEASON_GATE].num_server = SEASON_GATE_SERVERS;
    blocks[GREEN_PASS].num_server = GREEN_PASS_SERVERS;

    init_temperature_ctrl();
    init_tickets_buy();

    clock.current = START;
    clock.arrival = getArrival(clock.current);
}

// Inizializza tutti i serventi di tutti i blocchi della rete
void init_blocks() {
    for (int block_type = 0; block_type <= NUM_BLOCKS; block_type++) {
        int servers;
        switch (block_type) {
            case TEMPERATURE_CTRL:
                servers = TEMPERATURE_CTRL_SERVERS;
                break;
            case TICKET_BUY:
                servers = TICKET_BUY_SERVERS;
                break;
            case TICKET_GATE:
                servers = TICKET_GATE_SERVERS;
                break;
            case SEASON_GATE:
                servers = SEASON_GATE_SERVERS;
                break;
            case GREEN_PASS:
                servers = GREEN_PASS_SERVERS;
                break;
            default:
                break;
        }

        server *head = (server *)malloc(sizeof(server));
        head->id = 1;
        head->nodeType = block_type;
        head->completion = INFINITY;
        head->stream = streamID++;
        blocks[block_type].firstServer = head;

        for (int i = 1; i < servers; i++) {
            server *s = (server *)malloc(sizeof(server));
            s->id = i + 1;
            s->status = 0;
            s->nodeType = block_type;
            s->completion = INFINITY;
            s->stream = streamID++;
            server *last = iterateOver(blocks[block_type].firstServer);
            last->next = s;
        }
    }
}