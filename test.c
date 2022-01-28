#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "DES/rngs.h"
#include "DES/rvgs.h"
#include "config.h"
#include "math.h"
#include "utils.h"

double getArrival(double current);
void enqueue(struct block *block, double arrival);
struct job dequeue(struct block *block);
server *findFreeServer(struct block b);
double findNextEvent(double nextArrival, struct block *services, server **server_completion);
double getService(enum block_types type, int stream);
void process_arrival();
void process_completion(compl completion);
void init_network();
void init_blocks();
void set_time_slot();
void activate_servers();

int time_slot;
bool slot_switched[3];
network_configuration config;

int streamID;                                  // Stream da selezionare per generare il tempo di servizio
server *nextCompletion;                        // Tiene traccia del server relativo al completamento imminente
sorted_completions global_sorted_completions;  // Tiene in una lista ordinata tutti i completamenti nella rete così da ottenere il prossimo in O(log(N))
network_status global_network_status;

int completed = 0;
int dropped = 0;

struct block blocks[NUM_BLOCKS];
struct clock_t clock;

double arrival_rate;

network_configuration get_config(int *values_1, int *values_2, int *values_3) {
    network_configuration *config = malloc(sizeof(network_configuration));
    for (int i = 0; i < NUM_BLOCKS; i++) {
        config->slot_config[0][i] = values_1[i];
        config->slot_config[1][i] = values_2[i];
        config->slot_config[2][i] = values_3[i];
    }
    return *config;
}

void print_network_status() {
    for (int j = 0; j < NUM_BLOCKS; j++) {
        for (int i = 0; i < MAX_SERVERS; i++) {
            server s = global_network_status.server_list[j][i];
            printf("(%d,%d) | status: {%d,%d}\n", s.block->type, s.id, s.status, s.online);
        }
    }
}

int main() {
    //debug_routing();
    init_network();

    // Gestione degli arrivi e dei completamenti
    while (clock.arrival <= TIME_SLOT_1 + TIME_SLOT_2 + TIME_SLOT_3) {
        //clearScreen();
        set_time_slot();
        printf(" \n========== NEW STEP ==========\n");
        printf("Prossimo arrivo: %f\n", clock.arrival);
        printf("Clock corrente: %f\n", clock.current);
        compl *nextCompletion = &global_sorted_completions.sorted_list[0];
        server *nextCompletionServer = nextCompletion->server;
        printf("Next Completion: (%d,%d),%f\n", nextCompletion->server->block->type, nextCompletion->server->id, nextCompletion->value);

        clock.next = min(nextCompletion->value, clock.arrival);  // Ottengo il prossimo evento
        for (int i = 0; i < NUM_BLOCKS; i++) {
            blocks[i].area += (clock.next - clock.current) * blocks[i].jobInBlock;
        }

        clock.current = clock.next;  // Avanzamento del clock al valore del prossimo evento
        printf("Clock next Event: %f\n", clock.next);

        // Gestione arrivo dall'esterno, quindi in TEMPERATURE_CTRL
        if (clock.current == clock.arrival) {
            process_arrival();
        }

        // Gestione Completamento
        else {
            process_completion(*nextCompletion);
        }
        // print_completions_status(&global_sorted_completions, blocks, dropped, completed);
    }
    print_completions_status(&global_sorted_completions, blocks, dropped, completed);

    for (int i = 0; i < NUM_BLOCKS; i++) {
        // printf("\navg wait for block #%d........... = %6.2f\n", i, blocks[i].area / blocks[i].total_arrivals);
        printf("avg  # in queue in block #%d........... = %6.2f\n", i, blocks[i].area / clock.current);
    }
}

/*
* Genera un tempo di Arrivo secondo la distribuzione specificata
* //TODO gestire le fascie orarie
*/
double getArrival(double current) {
    double arrival = current;
    SelectStream(254);
    arrival += Exponential(1 / arrival_rate);
    return (arrival);
}

// Inserisce un job nella coda del blocco specificata
void enqueue(struct block *block, double arrival) {
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
struct job dequeue(struct block *block) {
    struct job *j = block->head;

    if (!j->next)
        block->tail = NULL;

    block->head = j->next;
    free(j);
}

// Ritorna un server libero nella Linked List del blocco
server *findFreeServer(struct block b) {
    int block_type = b.type;
    int active_servers = global_network_status.num_online_servers[block_type];
    for (int i = 0; i < active_servers; i++) {
        if (global_network_status.server_list[block_type][i].status == IDLE) {
            return &global_network_status.server_list[block_type][i];
        }
    }
    return NULL;
}

// Genera un tempo di servizio secondo la distribuzione specificata e stream del servente individuato
double getService(enum block_types type, int stream) {
    SelectStream(stream);

    switch (type) {
        case TEMPERATURE_CTRL:
            return Exponential(SERV_TEMPERATURE_CTRL);
        case TICKET_BUY:
            return Exponential(SERV_TICKET_BUY);
        case TICKET_GATE:
            return Exponential(SERV_TICKET_GATE);
        case SEASON_GATE:
            return Exponential(SERV_SEASON_GATE);
        case GREEN_PASS:
            return Exponential(SERV_GREEN_PASS);
        default:
            return 0;
    }
}
/*
Processa un arrivo dall'esterno
*/
void process_arrival() {
    printf("\nProcessamento di un Arrivo dall'esterno\n");
    blocks[TEMPERATURE_CTRL].total_arrivals++;
    blocks[TEMPERATURE_CTRL].jobInBlock++;

    server *s = findFreeServer(blocks[TEMPERATURE_CTRL]);

    // C'è un servente libero, quindi genero il completamento
    if (s != NULL) {
        printf("C'è un servente libero nel controllo temperatura: %d\n", s->id);
        double serviceTime = getService(TEMPERATURE_CTRL, s->stream);
        compl c = {s, INFINITY};
        c.value = clock.current + serviceTime;
        s->status = BUSY;  // Setto stato busy
        insertSorted(&global_sorted_completions, c);
    } else {
        printf("Tutti i serventi nel controllo temperatura sono BUSY. Job accodato\n");
        blocks[TEMPERATURE_CTRL].jobInQueue++;  // Se non c'è un servente libero aumenta il numero di job in coda
    }
    enqueue(&blocks[TEMPERATURE_CTRL], clock.arrival);  // lo appendo nella coda del blocco TEMP
    clock.arrival = getArrival(clock.current);          // Genera prossimo arrivo
}

// Processa un next-event di completamento
void process_completion(compl c) {
    int block_type = c.server->block->type;
    blocks[block_type].total_completions++;
    blocks[block_type].jobInBlock--;

    int destination;
    server *freeServer;
    printf("\nProcessamento di un Completamento sul Blocco #%d\n", block_type);

    dequeue(&blocks[block_type]);  // Toglie il job servito dal blocco e fa "avanzare" la lista collegata di job
    deleteElement(&global_sorted_completions, c);

    // Se nel blocco temperatura ci sono job in coda, devo generare il prossimo completamento per il servente che si è liberato.
    if (blocks[block_type].jobInQueue > 0) {
        printf("C'è un job in coda nel blocco %d. Il server %d và in BUSY\n", c.server->block->type, c.server->id);

        blocks[block_type].jobInQueue--;
        c.value = clock.current + getService(block_type, c.server->stream);
        insertSorted(&global_sorted_completions, c);
    } else {
        printf("Nessun job in coda nel blocco %d. Il server %d và in IDLE\n", c.server->block->type, c.server->id);
        c.server->status = IDLE;
    }

    if (block_type == GREEN_PASS) {
        // Il Job è completato ed esce dal sistema
        completed++;
        return;
    }

    // Gestione blocco destinazione
    destination = getDestination(c.server->block->type);  // Trova la destinazione adatta per il job appena servito
    printf("Inoltro il job al blocco destinatario: #%d\n", destination);
    if (destination == EXIT) {
        // Il job viene scartato
        dropped++;
        return;
    }
    blocks[destination].total_arrivals++;
    blocks[destination].jobInBlock++;
    enqueue(&blocks[destination], c.value);  // Posiziono il job nella coda del blocco destinazione e gli imposto come tempo di arrivo quello di completamento

    // Se il blocco destinatario ha un servente libero, generiamo un tempo di completamento, altrimenti aumentiamo il numero di job in coda
    freeServer = findFreeServer(blocks[destination]);
    if (freeServer != NULL) {
        compl c2 = {freeServer, INFINITY};
        c2.value = clock.current + getService(destination, freeServer->stream);
        insertSorted(&global_sorted_completions, c2);
        freeServer->status = BUSY;
    } else {
        blocks[destination].jobInQueue++;
    }
}

// Inizializza tutti i blocchi del sistema
void init_network() {
    printf("Initializing Network\n");
    PlantSeeds(5234234);
    streamID = 0;
    slot_switched[0] = false;
    slot_switched[1] = false;
    slot_switched[2] = false;

    int slot1_conf[] = {3, 20, 1, 5, 15};
    int slot2_conf[] = {5, 45, 4, 15, 25};
    int slot3_conf[] = {2, 10, 1, 3, 15};

    config = get_config(slot1_conf, slot2_conf, slot3_conf);

    init_blocks();
    set_time_slot();

    clock.current = START;
    clock.arrival = getArrival(clock.current);
    global_sorted_completions.num_completions = 0;
}

// Inizializza tutti i serventi di tutti i blocchi della rete
void init_blocks() {
    printf("Initilizing servers\n");
    for (int block_type = 0; block_type < NUM_BLOCKS; block_type++) {
        blocks[block_type].type = block_type;
        blocks[block_type].jobInBlock = 0;
        blocks[block_type].jobInQueue = 0;
        blocks[block_type].total_arrivals = 0;
        blocks[block_type].total_completions = 0;

        for (int i = 0; i < MAX_SERVERS; i++) {
            server s;
            s.id = i;
            s.status = IDLE;
            s.online = OFFLINE;
            s.block = &blocks[block_type];
            s.stream = streamID++;

            global_network_status.server_list[block_type][i] = s;

            compl c = {&global_network_status.server_list[block_type][i], INFINITY};
            insertSorted(&global_sorted_completions, c);
        }
    }
}

void set_time_slot() {
    if (clock.current < TIME_SLOT_1 && !slot_switched[0]) {
        time_slot = 0;
        arrival_rate = LAMBDA_1;
        activate_servers();
        slot_switched[0] = true;
    }
    if (clock.current >= TIME_SLOT_1 && clock.current < TIME_SLOT_1 + TIME_SLOT_2 && !slot_switched[1]) {
        time_slot = 1;
        printf("CAMBIO FASCIA a 2!");
        arrival_rate = LAMBDA_2;
        activate_servers();
        sleep(5);
        slot_switched[1] = true;
    }
    if (clock.current >= TIME_SLOT_1 + TIME_SLOT_2 && !slot_switched[2]) {
        time_slot = 2;
        printf("CAMBIO FASCIA a 3!");
        arrival_rate = LAMBDA_3;
        activate_servers();
        sleep(5);
        slot_switched[2] = true;
    }
}

void activate_servers() {
    for (int j = 0; j < NUM_BLOCKS; j++) {
        for (int i = 0; i < config.slot_config[time_slot][j]; i++) {
            global_network_status.server_list[j][i].online = ONLINE;
            global_network_status.num_online_servers[j]++;
        }
    }
}

void deactivate_servers() {
}