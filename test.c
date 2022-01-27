#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "DES/rngs.h"
#include "DES/rvgs.h"
#include "config.h"
#include "math.h"
#include "utils.h"

//TODO aggiustare gli InsertSorted dopo i GetService

double getArrival(double current);
void enqueue(struct node *block, double arrival);
struct job dequeue(struct node *block);
server *findFreeServer(server *block_head);
double findNextEvent(double nextArrival, struct node *services, server **server_completion);
double getService(enum node_type type, int stream);
void process_arrival();
void process_completion(compl completion);
void init_network();
void init_blocks();

int streamID;                                  // Stream da selezionare per generare il tempo di servizio
server *nextCompletion;                        // Tiene traccia del server relativo al completamento imminente
sorted_completions global_sorted_completions;  // Tiene in una lista ordinata tutti i completamenti nella rete così da ottenere il prossimo in O(log(N))

struct node blocks[NUM_BLOCKS];
struct clock_t clock;

void debug_test_sorted() {
    compl s1 = {0, 5.231};
    compl s2 = {0, 1.231};
    compl s3 = {0, 56.231};
    compl s4 = {0, 15.231};

    sorted_completions sorted_compls;

    clearScreen();
    insertSorted(&sorted_compls, s1);
    printf("List Status: %d | {%f , %f , %f , %f}\n\n", sorted_compls.num_completions, sorted_compls.sorted_list[0].value, sorted_compls.sorted_list[1].value, sorted_compls.sorted_list[2].value, sorted_compls.sorted_list[3].value);
    insertSorted(&sorted_compls, s2);
    printf("List Status: %d | {%f , %f , %f , %f}\n\n", sorted_compls.num_completions, sorted_compls.sorted_list[0].value, sorted_compls.sorted_list[1].value, sorted_compls.sorted_list[2].value, sorted_compls.sorted_list[3].value);
    insertSorted(&sorted_compls, s3);
    printf("List Status: %d | {%f , %f , %f , %f}\n\n", sorted_compls.num_completions, sorted_compls.sorted_list[0].value, sorted_compls.sorted_list[1].value, sorted_compls.sorted_list[2].value, sorted_compls.sorted_list[3].value);
    insertSorted(&sorted_compls, s4);
    printf("List Status: %d | {%f , %f , %f , %f}\n\n", sorted_compls.num_completions, sorted_compls.sorted_list[0].value, sorted_compls.sorted_list[1].value, sorted_compls.sorted_list[2].value, sorted_compls.sorted_list[3].value);
    deleteElement(&sorted_compls, s1);
    printf("List Status: %d | {%f , %f , %f , %f}\n\n", sorted_compls.num_completions, sorted_compls.sorted_list[0].value, sorted_compls.sorted_list[1].value, sorted_compls.sorted_list[2].value, sorted_compls.sorted_list[3].value);
}

int main() {
    //debug_test_sorted();
    init_network();

    // Gestione degli arrivi e dei completamenti
    while (clock.arrival <= STOP) {
        clearScreen();
        printf("Prossimo arrivo: %f\n", clock.arrival);
        printf("Clock corrente: %f\n", clock.current);
        compl *nextCompletion = &global_sorted_completions.sorted_list[0];
        server *nextCompletionServer = nextCompletion->server;

        clock.next = min(nextCompletion->value, clock.arrival);  // Ottengo il prossimo evento
        clock.current = clock.next;                              // Avanzamento del clock al valore del prossimo evento

        printf("Clock next Event: %f\n", clock.next);

        // Gestione arrivo dall'esterno, quindi in TEMPERATURE_CTRL
        if (clock.current == clock.arrival) {
            process_arrival();
        }

        // Gestione Completamento
        else {
            process_completion(*nextCompletion);
        }
    }
    print_completions_status(&global_sorted_completions, global_sorted_completions.num_completions, blocks);
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
void enqueue(struct node *block, double arrival) {
    struct job *j = (struct job *)malloc(sizeof(struct job));
    if (j == NULL)
        handle_error("malloc");

    j->arrival = arrival;
    j->next = NULL;

    if (block->tail)  // Appendi alla coda se esiste, altrimenti è la testa
        block->tail->next = j;
    else
        block->head = j;

    block->tail = j;
}

// Ritorna e rimuove il job dalla coda del blocco specificata
struct job dequeue(struct node *block) {
    struct job *j = block->head;

    if (!j->next)
        block->tail = NULL;

    block->head = j->next;
    free(j);
}

// Ritorna un server libero nella Linked List del blocco
server *findFreeServer(server *block_head) {
    server *current = block_head;
    while (current != NULL) {
        if (current->status == 0) return current;
        if (current->next != NULL) {
            current = current->next;
        } else
            break;
    }
    return NULL;
}

// Genera un tempo di servizio secondo la distribuzione specificata e stream del servente individuato
double getService(enum node_type type, int stream) {
    SelectStream(stream);
    double x;

    switch (type) {
        case TEMPERATURE_CTRL:
            return Exponential(0.9);
        case TICKET_BUY:
            return Exponential(18);
        case TICKET_GATE:
            return Exponential(4);
        case GREEN_PASS:
            return Exponential(10);
        case SEASON_GATE:
            return Exponential(3);
        default:
            return 0;
    }
}

/*
Processa un arrivo dall'esterno
*/
void process_arrival() {
    printf("\nProcessamento di un Arrivo\n");
    server *s = findFreeServer(blocks[TEMPERATURE_CTRL].firstServer);

    // C'è un servente libero
    if (s != NULL) {
        double serviceTime = getService(TEMPERATURE_CTRL, s->stream);
        compl c = {s, INFINITY};
        c.value = clock.current + serviceTime;
        s->status = BUSY;  // Setto stato busy
        insertSorted(&global_sorted_completions, c);
    } else {
        blocks[TEMPERATURE_CTRL].jobInQueue++;  // Se non c'è un servente libero aumenta il numero di job in coda
    }
    enqueue(&blocks[TEMPERATURE_CTRL], clock.arrival);  // lo appendo nella coda del blocco TEMP
    clock.arrival = getArrival(clock.current);          // Genera prossimo arrivo
}

void process_completion(compl c) {
    enum node_type destination;
    server *freeServer;
    printf("\nProcessamento di un Completamento\n");

    switch (c.server->nodeType) {
        case TEMPERATURE_CTRL:;
            struct job j = dequeue(&blocks[TEMPERATURE_CTRL]);  // Toglie il job servito dal blocco e fa "avanzare" la lista collegata di job
            deleteElement(&global_sorted_completions, c);

            // Se nel blocco temperatura ci sono job in coda, devo generare il prossimo completamento per il servente che si è liberato.
            if (blocks[TEMPERATURE_CTRL].jobInQueue > 0) {
                blocks[TEMPERATURE_CTRL].jobInQueue--;
                c.value = clock.current + getService(TEMPERATURE_CTRL, c.server->stream);
                insertSorted(&global_sorted_completions, c);
            } else {
                printf("Nessun job in coda nel blocco %d. Il server %d và in IDLE\n", c.server->nodeType, c.server->id);
                c.server->status = IDLE;
            }
            printf("Inoltro il job al destinatario\n");

            // Gestione blocco destinazione
            destination = getDestination(c.server->nodeType);  // Trova la destinazione adatta per il job appena servito
            enqueue(&blocks[destination], c.value);            // Posiziono il job nella coda del blocco destinazione e gli imposto come tempo di arrivo quello di completamento

            // Se il blocco destinatario ha un servente libero, generiamo un tempo di completamento, altrimenti aumentiamo il numero di job in coda
            freeServer = findFreeServer(blocks[destination].firstServer);
            if (freeServer != NULL) {
                compl c2 = {freeServer, INFINITY};
                c2.value = clock.current + getService(destination, freeServer->stream);
                insertSorted(&global_sorted_completions, c2);
                freeServer->status = BUSY;
            } else {
                blocks[destination].jobInQueue++;
            }
            break;

        case TICKET_BUY:
            dequeue(&blocks[TICKET_BUY]);
            deleteElement(&global_sorted_completions, c);
            if (blocks[TICKET_BUY].jobInQueue > 0) {
                blocks[TICKET_BUY].jobInQueue--;
                c.value = clock.current + getService(TICKET_BUY, c.server->stream);
                insertSorted(&global_sorted_completions, c);
            } else {
                c.server->status = IDLE;
            }

            destination = getDestination(c.server->nodeType);
            enqueue(&blocks[destination], c.value);
            freeServer = findFreeServer(blocks[destination].firstServer);
            if (freeServer != NULL) {
                compl c2 = {freeServer, INFINITY};
                c2.value = clock.current + getService(destination, freeServer->stream);
                insertSorted(&global_sorted_completions, c2);
                freeServer->status = BUSY;
            } else {
                blocks[destination].jobInQueue++;
            }
            break;

        case SEASON_GATE:
            dequeue(&blocks[SEASON_GATE]);
            deleteElement(&global_sorted_completions, c);
            if (blocks[SEASON_GATE].jobInQueue > 0) {
                blocks[SEASON_GATE].jobInQueue--;
                c.value = clock.current + getService(SEASON_GATE, c.server->stream);
                insertSorted(&global_sorted_completions, c);
            } else {
                c.server->status = IDLE;
            }

            destination = getDestination(c.server->nodeType);
            enqueue(&blocks[destination], c.value);
            freeServer = findFreeServer(blocks[destination].firstServer);
            if (freeServer != NULL) {
                compl c2 = {freeServer, INFINITY};
                c2.value = clock.current + getService(destination, freeServer->stream);
                insertSorted(&global_sorted_completions, c2);
                freeServer->status = BUSY;
            } else {
                blocks[destination].jobInQueue++;
            }
            break;

        case TICKET_GATE:
            dequeue(&blocks[TICKET_GATE]);
            deleteElement(&global_sorted_completions, c);
            if (blocks[TICKET_GATE].jobInQueue > 0) {
                blocks[TICKET_GATE].jobInQueue--;
                c.value = clock.current + getService(TICKET_GATE, c.server->stream);
                insertSorted(&global_sorted_completions, c);
            } else {
                c.server->status = IDLE;
            }

            destination = getDestination(c.server->nodeType);
            enqueue(&blocks[destination], c.value);
            freeServer = findFreeServer(blocks[destination].firstServer);
            if (freeServer != NULL) {
                compl c2 = {freeServer, INFINITY};
                c2.value = clock.current + getService(destination, freeServer->stream);
                insertSorted(&global_sorted_completions, c2);
                freeServer->status = BUSY;
            } else {
                blocks[destination].jobInQueue++;
            }
            break;

        case GREEN_PASS:
            dequeue(&blocks[GREEN_PASS]);
            deleteElement(&global_sorted_completions, c);
            if (blocks[GREEN_PASS].jobInQueue > 0) {
                blocks[GREEN_PASS].jobInQueue--;
                c.value = clock.current + getService(GREEN_PASS, c.server->stream);
                insertSorted(&global_sorted_completions, c);
            } else {
                c.server->status = IDLE;
            }
            break;

        default:
            break;
    }
}

// Inizializza tutti i blocchi del sistema
void init_network() {
    printf("Initializing Network\n");
    PlantSeeds(1293829);

    streamID = 0;

    blocks[TEMPERATURE_CTRL].num_server = TEMPERATURE_CTRL_SERVERS;
    blocks[TICKET_BUY].num_server = TICKET_BUY_SERVERS;
    blocks[TICKET_GATE].num_server = TICKET_GATE_SERVERS;
    blocks[SEASON_GATE].num_server = SEASON_GATE_SERVERS;
    blocks[GREEN_PASS].num_server = GREEN_PASS_SERVERS;
    init_blocks();

    clock.current = START;
    clock.arrival = getArrival(clock.current);
    global_sorted_completions.num_completions = 0;
}

// Inizializza tutti i serventi di tutti i blocchi della rete
void init_blocks() {
    printf("Initilizing servers\n");
    for (int block_type = 0; block_type < NUM_BLOCKS; block_type++) {
        printf("%d, %d\n", block_type, NUM_BLOCKS);
        int servers;

        server *head = (server *)malloc(sizeof(server));
        head->id = 1;
        head->nodeType = block_type;
        head->stream = streamID++;
        compl c = {head, INFINITY};

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
        blocks[block_type].firstServer = head;
        blocks[block_type].type = block_type;
        server *last = head;

        for (int i = 1; i < servers; i++) {
            server *s = (server *)malloc(sizeof(server));
            s->id = i + 1;
            s->status = 0;
            s->nodeType = block_type;
            s->stream = streamID++;
            last->next = s;
            compl c = {last, INFINITY};
            insertSorted(&global_sorted_completions, c);
            last = s;
        }
        compl c2 = {last, INFINITY};
        insertSorted(&global_sorted_completions, c2);
    }
}