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
void deactivate_servers();
void update_network();

int time_slot;
bool slot_switched[3];
network_configuration config;

int streamID;                                  // Stream da selezionare per generare il tempo di servizio
server *nextCompletion;                        // Tiene traccia del server relativo al completamento imminente
sorted_completions global_sorted_completions;  // Tiene in una lista ordinata tutti i completamenti nella rete così da ottenere il prossimo in O(log(N))
network_status global_network_status;

int completed = 0;
int dropped = 0;
int bypassed = 0;

struct block blocks[NUM_BLOCKS];
struct clock_t clock;

double arrival_rate;

int S1 = TIME_SLOT_1;
int S2 = TIME_SLOT_1 + TIME_SLOT_2;
int S3 = TIME_SLOT_1 + TIME_SLOT_2 + TIME_SLOT_3;

int main(int argc, char *argv[]) {
    int m = atoi(argv[1]);
    int stop_simulation = 0;

    if (argc == 2) {
        switch (m) {
            case 1:
                stop_simulation = S1;
                break;
            case 2:
                stop_simulation = S2;
                break;
            default:
                stop_simulation = S3;
        }
    } else {
        stop_simulation = S3;
    }
    //debug_routing();
    init_network();

    // Gestione degli arrivi e dei completamenti
    while (clock.arrival <= stop_simulation) {
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
            if (blocks[i].jobInBlock > 0) {
                blocks[i].area.node += (clock.next - clock.current) * blocks[i].jobInBlock;
                blocks[i].area.queue += (clock.next - clock.current) * blocks[i].jobInQueue;
                //blocks[i].area.service += (clock.next - clock.current);  //TODO Utilizzazione non si calcola cosi, va calcolata per ogni server. La mettiamo a implementazione array server completata
            }
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
        //print_block_status(&global_sorted_completions, blocks, dropped, completed,bypassed);
    }
    print_cost_details(config);
    print_block_status(&global_sorted_completions, blocks, dropped, completed, bypassed);
    //print_network_status(&global_network_status);
    print_statistics(&global_network_status, blocks, clock.current, &global_sorted_completions);
}

/*
* Genera un tempo di Arrivo secondo la distribuzione specificata
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
        block->head_service = j;

    block->tail = j;

    if (block->head_queue == NULL) {
        block->head_queue = j;
    }
}

// Ritorna e rimuove il job dalla coda del blocco specificata
struct job dequeue(struct block *block) {
    struct job *j = block->head_service;

    if (!j->next)
        block->tail = NULL;

    block->head_service = j->next;

    if (block->head_queue != NULL && block->head_queue->next != NULL) {
        struct job *tmp = block->head_queue->next;
        block->head_queue = tmp;
    }

    else {
        block->head_queue = NULL;
    }

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
        s->sum.service += serviceTime;
        s->block->area.service += serviceTime;
        s->sum.served++;
        insertSorted(&global_sorted_completions, c);
        enqueue(&blocks[TEMPERATURE_CTRL], clock.arrival);  // lo appendo nella coda del blocco TEMP
    } else {
        enqueue(&blocks[TEMPERATURE_CTRL], clock.arrival);  // lo appendo nella coda del blocco TEMP
        printf("Tutti i serventi nel controllo temperatura sono BUSY. Job accodato\n");
        blocks[TEMPERATURE_CTRL].jobInQueue++;  // Se non c'è un servente libero aumenta il numero di job in coda
    }
    clock.arrival = getArrival(clock.current);  // Genera prossimo arrivo
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

    // Se nel blocco ci sono job in coda, devo generare il prossimo completamento per il servente che si è liberato.
    if (blocks[block_type].jobInQueue > 0 && !c.server->need_resched) {
        printf("C'è un job in coda nel blocco %d. Il server %d và in BUSY\n", c.server->block->type, c.server->id);

        blocks[block_type].jobInQueue--;
        double service_1 = getService(block_type, c.server->stream);
        c.value = clock.current + service_1;
        c.server->sum.service += service_1;
        c.server->sum.served++;
        c.server->block->area.service += service_1;

        insertSorted(&global_sorted_completions, c);
    } else {
        printf("Nessun job in coda nel blocco %d. Il server %d và in IDLE\n", c.server->block->type, c.server->id);
        c.server->status = IDLE;
    }

    if (c.server->need_resched) {
        c.server->online = OFFLINE;
        c.server->need_resched = false;
    }

    // Il Job è completato ed esce dal sistema
    if (block_type == GREEN_PASS) {
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
    if (destination != GREEN_PASS) {
        blocks[destination].total_arrivals++;
        blocks[destination].jobInBlock++;
        enqueue(&blocks[destination], c.value);  // Posiziono il job nella coda del blocco destinazione e gli imposto come tempo di arrivo quello di completamento

        // Se il blocco destinatario ha un servente libero, generiamo un tempo di completamento, altrimenti aumentiamo il numero di job in coda
        freeServer = findFreeServer(blocks[destination]);
        if (freeServer != NULL) {
            compl c2 = {freeServer, INFINITY};
            double service_2 = getService(destination, freeServer->stream);
            c2.value = clock.current + service_2;
            insertSorted(&global_sorted_completions, c2);
            freeServer->status = BUSY;
            freeServer->sum.service += service_2;
            freeServer->sum.served++;
            freeServer->block->area.service += service_2;

            return;
        } else {
            blocks[destination].jobInQueue++;
            return;
        }
    }

    // Desination == GREEN_PASS
    blocks[destination].total_arrivals++;
    freeServer = findFreeServer(blocks[destination]);
    if (freeServer != NULL) {
        blocks[destination].jobInBlock++;
        enqueue(&blocks[destination], c.value);  // Posiziono il job nella coda del blocco destinazione e gli imposto come tempo di arrivo quello di completamento

        compl c3 = {freeServer, INFINITY};
        double service_3 = getService(destination, freeServer->stream);
        c3.value = clock.current + service_3;
        insertSorted(&global_sorted_completions, c3);
        freeServer->status = BUSY;
        freeServer->sum.service += service_3;
        freeServer->sum.served++;
        freeServer->block->area.service += service_3;

        return;
    } else {
        completed++;
        bypassed++;
        blocks[GREEN_PASS].total_bypassed++;
        return;
    }
}

// Inizializza tutti i blocchi del sistema
void init_network() {
    printf("Initializing Network\n");
    PlantSeeds(521312312);
    streamID = 0;
    slot_switched[0] = false;
    slot_switched[1] = false;
    slot_switched[2] = false;

    // Config 1
    int slot1_conf[] = {3, 20, 1, 5, 15};
    int slot2_conf[] = {5, 45, 4, 15, 25};
    int slot3_conf[] = {2, 10, 1, 3, 15};

    // Config_2
    int slot1_conf_2[] = {10, 30, 3, 20, 15};
    int slot2_conf_2[] = {15, 45, 4, 15, 20};
    int slot3_conf_2[] = {12, 20, 2, 40, 10};

    //config = get_config(slot1_conf, slot2_conf, slot3_conf);
    config = get_config(slot1_conf_2, slot2_conf_2, slot3_conf_2);

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
        blocks[block_type].total_bypassed = 0;

        for (int i = 0; i < MAX_SERVERS; i++) {
            server s;
            s.id = i;
            s.status = IDLE;
            s.online = OFFLINE;
            s.need_resched = false;
            s.block = &blocks[block_type];
            s.stream = streamID++;
            s.sum.served = 0;
            s.sum.service = 0.0;
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
        slot_switched[0] = true;
    }
    if (clock.current >= TIME_SLOT_1 && clock.current < TIME_SLOT_1 + TIME_SLOT_2 && !slot_switched[1]) {
        time_slot = 1;
        arrival_rate = LAMBDA_2;
        slot_switched[1] = true;
    }
    if (clock.current >= TIME_SLOT_1 + TIME_SLOT_2 && !slot_switched[2]) {
        time_slot = 2;
        arrival_rate = LAMBDA_3;
        slot_switched[2] = true;
    }
    update_network();
}

// Aggiorna i serventi attivi al cambio di fascia, attivando o disattivando il numero necessario per ogni blocco
void update_network() {
    int actual, new = 0;
    for (int j = 0; j < NUM_BLOCKS; j++) {
        actual = global_network_status.num_online_servers[j];
        new = config.slot_config[time_slot][j];

        if (actual > new) {
            deactivate_servers(j);
        } else if (actual < new) {
            activate_servers(j);
        }
    }
}

// Attiva un certo numero di server per il blocco, fino al numero specificato dalla configurazione
void activate_servers(int block) {
    int start = 0;
    start = global_network_status.num_online_servers[block];
    for (int i = start; i < config.slot_config[time_slot][block]; i++) {
        server *s = &global_network_status.server_list[block][i];
        s->online = ONLINE;
        s->used = USED;
        if (blocks[block].jobInQueue > 0) {
            if (blocks[block].head_queue->next != NULL) {
                struct job *tmp = blocks[block].head_queue->next;
                blocks[block].head_queue = tmp;
            } else {
                blocks[block].head_queue = NULL;
            }
            double serviceTime = getService(block, s->stream);
            compl c = {s, INFINITY};
            s->status = BUSY;
            c.value = clock.current + serviceTime;
            blocks[block].jobInQueue--;
            s->block->area.service += serviceTime;
            s->sum.service += serviceTime;

            insertSorted(&global_sorted_completions, c);
        }
        global_network_status.num_online_servers[block] = config.slot_config[time_slot][block];
    }
    //print_network_status();
    print_block_status(&global_sorted_completions, blocks, dropped, completed, bypassed);
}

// Disattiva un certo numero di server per il blocco, fino al numero specificato dalla configurazione
void deactivate_servers(int block) {
    int start = 0;
    start = global_network_status.num_online_servers[block];

    for (int i = start - 1; i >= config.slot_config[time_slot][block]; i--) {
        server *s = &global_network_status.server_list[block][i];

        if (s->status == BUSY) {
            s->need_resched = true;
        } else {
            s->online = OFFLINE;
        }
        global_network_status.num_online_servers[block] = config.slot_config[time_slot][block];
    }
}