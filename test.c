#include <stdio.h>
#include <stdlib.h>

#include "DES/rngs.h"
#include "DES/rvgs.h"
#include "config.h"
#include "math.h"
#include "structures.h"
#include "utils.h"

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
                compl ->completion = getService(TICKET_BUY, compl ->stream);

            break;

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