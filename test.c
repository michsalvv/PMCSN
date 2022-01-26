#include <stdio.h>
#include <stdlib.h>

#include "DES/rngs.h"
#include "DES/rvgs.h"
#include "config.h"
#include "math.h"
#include "utils.h"

double getArrival(double current);
void enqueue(struct node *block, double arrival);
struct job dequeue(struct node *block);
server *findFreeServer(server *block_head);
double findNextEvent(double nextArrival, struct node *services, server **server_completion);
double getService(enum node_type type, int stream);
void process_arrival();
void process_completion(server * compl );
void init_network();
void init_blocks();

int streamID;                           // Stream da selezionare per generare il tempo di servizio
server *nextCompletion;                 // Tiene traccia del server relativo al completamento imminente
sorted_completions global_completions;  // Tiene in una lista ordinata tutti i completamenti nella rete così da ottenere il prossimo in O(log(N))

struct node blocks[2];
struct clock_t clock;

void debug_test_sorted() {
    server s1 = {0, 0, 5.231};
    server s2 = {0, 1, 1.231};
    server s3 = {0, 2, 56.231};
    server s4 = {0, 3, 15.231};

    server init[TOTAL_SERVERS];
    sorted_completions sorted;
    *sorted.sorted = *init;
    sorted.num_completion = 0;
    clearScreen();
    insertSorted(&sorted, s1);
    printf("List Status: %d | {%f , %f , %f , %f}\n\n", sorted.num_completion, sorted.sorted[0].completion, sorted.sorted[1].completion, sorted.sorted[2].completion, sorted.sorted[3].completion);
    insertSorted(&sorted, s2);
    printf("List Status: %d | {%f , %f , %f , %f}\n\n", sorted.num_completion, sorted.sorted[0].completion, sorted.sorted[1].completion, sorted.sorted[2].completion, sorted.sorted[3].completion);
    insertSorted(&sorted, s3);
    printf("List Status: %d | {%f , %f , %f , %f}\n\n", sorted.num_completion, sorted.sorted[0].completion, sorted.sorted[1].completion, sorted.sorted[2].completion, sorted.sorted[3].completion);
    insertSorted(&sorted, s4);
    printf("List Status: %d | {%f , %f , %f , %f}\n\n", sorted.num_completion, sorted.sorted[0].completion, sorted.sorted[1].completion, sorted.sorted[2].completion, sorted.sorted[3].completion);
    deleteElement(&sorted, s1);
    printf("List Status: %d | {%f , %f , %f , %f}\n\n", sorted.num_completion, sorted.sorted[0].completion, sorted.sorted[1].completion, sorted.sorted[2].completion, sorted.sorted[3].completion);
}

int main() {
    //debug_test_sorted();
    init_network();
    printf("%d\n", global_completions.num_completion);

    // Gestione degli arrivi e dei completamenti
    while (clock.arrival <= STOP) {
        clearScreen();
        printf("Prossimo arrivo: %f\n", clock.arrival);
        printf("Clock corrente: %f\n", clock.current);
        nextCompletion = &global_completions.sorted[0];

        clock.next = min(nextCompletion->completion, clock.arrival);  // Ottengo il prossimo evento
        clock.current = clock.next;                                   // Avanzamento del clock al valore del prossimo evento

        printf("Clock next Event: %f\n", clock.next);
        printServerList(blocks[TEMPERATURE_CTRL]);
        printServerList(blocks[TICKET_BUY]);
        print_array(global_completions, TOTAL_SERVERS);

        // Gestione arrivo dall'esterno, quindi in TEMPERATURE_CTRL
        if (clock.current == clock.arrival) {
            process_arrival();
        }

        // Gestione Completamento
        else {
            process_completion(nextCompletion);
        }
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
            return Exponential(0.7);
        case GREEN_PASS:
            return Exponential(1);
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
    server *s = findFreeServer(&global_completions.block_heads[TEMPERATURE_CTRL]);

    // C'è un servente libero
    if (s != NULL) {
        double serviceTime = getService(TEMPERATURE_CTRL, s->stream);
        s->completion = clock.current + serviceTime;
        s->status = BUSY;  // Setto stato busy
        insertSorted(&global_completions, *s);

    } else {
        blocks[TEMPERATURE_CTRL].jobInQueue++;  // Se non c'è un servente libero aumenta il numero di job in coda
    }
    enqueue(&blocks[TEMPERATURE_CTRL], clock.arrival);  // lo appendo nella coda del blocco TEMP

    clock.arrival = getArrival(clock.current);  // Genera prossimo arrivo
}

void process_completion(server * compl ) {
    switch (compl ->nodeType) {
        case TEMPERATURE_CTRL:;
            struct job j = dequeue(&blocks[TEMPERATURE_CTRL]);  // Toglie il job servito dal blocco e fa "avanzare" la lista collegata di job
            deleteElement(&global_completions, *compl );

            // Se nel blocco temperatura ci sono job in coda, devo generare il prossimo completamento per il servente che si è liberato.
            if (blocks[TEMPERATURE_CTRL].jobInQueue > 0) {
                blocks[TEMPERATURE_CTRL].jobInQueue--;
                compl ->completion = clock.current + getService(TEMPERATURE_CTRL, compl ->stream);
                insertSorted(&global_completions, *compl );
            } else {
                compl ->completion = INFINITY;
                compl ->status = IDLE;
            }

            // Gestione blocco destinazione
            enum node_type destination = getDestination(compl ->nodeType);  // Trova la destinazione adatta per il job appena servito
            enqueue(&blocks[destination], compl ->completion);              // Posiziono il job nella coda del blocco destinazione e gli imposto come tempo di arrivo quello di completamento

            // Se il blocco destinatario ha un servente libero, generiamo un tempo di completamento, altrimenti aumentiamo il numero di job in coda
            server *freeServer = findFreeServer(&global_completions.block_heads[destination]);
            if (freeServer != NULL) {
                freeServer->completion = clock.current + getService(destination, freeServer->stream);
                freeServer->status = BUSY;
                insertSorted(&global_completions, *compl );
            } else {
                blocks[destination].jobInQueue++;
            }
            break;

        case TICKET_BUY:
            dequeue(&blocks[TICKET_BUY]);
            if (blocks[TICKET_BUY].jobInQueue > 0) {
                blocks[TICKET_BUY].jobInQueue--;
                compl ->completion = clock.current + getService(TICKET_BUY, compl ->stream);
                insertSorted(&global_completions, *compl );
            } else {
                compl ->completion = INFINITY;
                compl ->status = IDLE;
            }
            break;
        default:
            break;
    }
}

// Inizializza tutti i blocchi del sistema
void init_network() {
    printf("Initializing Network\n");
    streamID = 0;
    blocks[TEMPERATURE_CTRL].num_server = TEMPERATURE_CTRL_SERVERS;
    blocks[TICKET_BUY].num_server = TICKET_BUY_SERVERS;
    blocks[TICKET_GATE].num_server = TICKET_GATE_SERVERS;
    blocks[SEASON_GATE].num_server = SEASON_GATE_SERVERS;
    blocks[GREEN_PASS].num_server = GREEN_PASS_SERVERS;
    init_blocks();

    clock.current = START;
    clock.arrival = getArrival(clock.current);
    global_completions.num_completion = 0;
    printf("%d\n", global_completions.num_completion);
}

// Inizializza tutti i serventi di tutti i blocchi della rete
void init_blocks() {
    for (int block_type = 0; block_type <= NUM_BLOCKS; block_type++) {
        int servers;

        server *head = (server *)malloc(sizeof(server));
        head->id = 1;
        head->nodeType = block_type;
        head->completion = INFINITY;
        head->stream = streamID++;

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

        global_completions.block_heads[block_type] = *head;
        blocks[block_type].firstServer = &global_completions.block_heads[block_type];

        for (int i = 1; i < servers; i++) {
            server *s = (server *)malloc(sizeof(server));
            s->id = i + 1;
            s->status = 0;
            s->nodeType = block_type;
            s->completion = INFINITY;
            s->stream = streamID++;
            server *last = iterateOver(&global_completions.block_heads[block_type]);
            last->next = s;
        }
    }
}