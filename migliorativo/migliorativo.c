#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "../DES/rngs.h"
#include "../DES/rvgs.h"
#include "config_2.h"
#include "math.h"
#include "utils_2.h"

network_configuration config;
struct clock_t clock;
struct block blocks[NUM_BLOCKS];
sorted_completions global_sorted_completions;
network_status global_network_status;
static const sorted_completions empty_sorted;
static const network_status empty_network;

double arrival_rate;
double lambdas[] = {LAMBDA_1, LAMBDA_2, LAMBDA_3};
int completed;
int dropped;
int bypassed;
bool slot_switched[3];

double stop_time = TIME_SLOT_1 + TIME_SLOT_2 + TIME_SLOT_3;
int streamID;

double getArrival(double current) {
    double arrival = current;
    SelectStream(254);
    arrival += Poisson(1 / arrival_rate);
    return arrival;
}

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

void init_blocks() {
    for (int block_type = 0; block_type < NUM_BLOCKS; block_type++) {
        blocks[block_type].type = block_type;
        blocks[block_type].jobInBlock = 0;
        blocks[block_type].jobInQueue = 0;
        blocks[block_type].total_arrivals = 0;
        blocks[block_type].total_completions = 0;
        blocks[block_type].total_bypassed = 0;
        blocks[block_type].area.node = 0;
        blocks[block_type].area.service = 0;
        blocks[block_type].area.queue = 0;

        for (int i = 0; i < MAX_SERVERS; i++) {
            server s;
            s.id = i;
            s.status = IDLE;
            s.online = OFFLINE;
            s.used = NOTUSED;
            s.need_resched = false;
            s.block = &blocks[block_type];
            s.stream = streamID++;
            s.sum.served = 0;
            s.sum.service = 0.0;
            s.time_online = 0.0;
            s.last_online = 0.0;
            s.jobInQueue = 0;

            //TODO AGGIUNTI PER MIGLIORATIVO
            s.head_queue = NULL;
            s.head_service = NULL;
            s.tail = NULL;

            global_network_status.server_list[block_type][i] = s;

            compl c = {&global_network_status.server_list[block_type][i], INFINITY};
            insertSorted(&global_sorted_completions, c);
        }
    }
}

void deactivate_servers(int block) {
    int start = 0;
    int slot = global_network_status.time_slot;
    start = global_network_status.num_online_servers[block];

    for (int i = start - 1; i >= config.slot_config[slot][block]; i--) {
        server *s = &global_network_status.server_list[block][i];

        if (s->status == BUSY) {
            s->need_resched = true;
        } else {
            s->online = OFFLINE;
            s->time_online += (clock.current - s->last_online);
            s->last_online = clock.current;
        }
        global_network_status.num_online_servers[block] = config.slot_config[slot][block];
    }
}

void activate_servers(int block) {
    int start = 0;
    int slot = global_network_status.time_slot;

    start = global_network_status.num_online_servers[block];
    for (int i = start; i < config.slot_config[slot][block]; i++) {
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
        global_network_status.num_online_servers[block] = config.slot_config[slot][block];
    }
}

void update_network() {
    int actual, new = 0;
    int slot = global_network_status.time_slot;

    for (int j = 0; j < NUM_BLOCKS; j++) {
        actual = global_network_status.num_online_servers[j];
        new = config.slot_config[slot][j];
        if (actual > new) {
            deactivate_servers(j);
        } else if (actual < new) {
            activate_servers(j);
        }
    }
}

void set_time_slot() {
    if (clock.current == START) {
        global_network_status.time_slot = 0;
        arrival_rate = LAMBDA_1;
        slot_switched[0] = true;
        update_network();
    }
    if (clock.current >= TIME_SLOT_1 && clock.current < TIME_SLOT_1 + TIME_SLOT_2 && !slot_switched[1]) {
        // calculate_statistics_fin(&global_network_status, blocks, clock.current, response_times);

        global_network_status.time_slot = 1;
        arrival_rate = LAMBDA_2;
        slot_switched[1] = true;
        // update_network();
    }
    if (clock.current >= TIME_SLOT_1 + TIME_SLOT_2 && !slot_switched[2]) {
        // calculate_statistics_fin(&global_network_status, blocks, clock.current, response_times);

        global_network_status.time_slot = 2;
        arrival_rate = LAMBDA_3;
        slot_switched[2] = true;
        // update_network();
    }
}

void init_network() {
    streamID = 0;
    clock.current = START;
    for (int i = 0; i < 3; i++) {
        slot_switched[i] = false;
        // response_times[i] = 0;
    }

    init_blocks();
    // if (str_compare(simulation_mode, "FINITE") == 0) {
    set_time_slot();
    // }

    completed = 0;
    bypassed = 0;
    dropped = 0;
    clock.arrival = getArrival(clock.current);
    global_sorted_completions.num_completions = 0;
}

// Inserisce un job nella coda del server specificato
void enqueue(server *s, double arrival) {
    struct job *j = (struct job *)malloc(sizeof(struct job));
    if (j == NULL)
        handle_error("malloc");

    j->arrival = arrival;
    j->next = NULL;

    if (s->tail)  // Appendi alla coda se esiste, altrimenti è la testa
        s->tail->next = j;
    else
        s->head_service = j;

    s->tail = j;

    if (s->head_queue == NULL) {
        s->head_queue = j;
    }
}

// Rimuove il job dalla coda del server specificato
void dequeue(server *s) {
    struct job *j = s->head_service;

    if (!j->next)
        s->tail = NULL;

    s->head_service = j->next;

    if (s->head_queue != NULL && s->head_queue->next != NULL) {
        struct job *tmp = s->head_queue->next;
        s->head_queue = tmp;
    } else {
        s->head_queue = NULL;
    }
    free(j);
}

server *findShorterServer(struct block b) {
    int block_type = b.type;
    int active_servers = global_network_status.num_online_servers[block_type];
    server *shorterTail = &global_network_status.server_list[block_type][0];

    if (block_type == GREEN_PASS) {
        // Nel blocco green pass non ci sono code, quindi bisogna trovare soltanto il server idle
        for (int j = 0; j < active_servers; j++) {
            if (global_network_status.server_list[block_type][j].status == IDLE) {
                return &global_network_status.server_list[block_type][j];
            }
            return NULL;
        }
    }

    for (int i = 0; i < active_servers; i++) {
        if (global_network_status.server_list[block_type][i].jobInQueue <= shorterTail->jobInQueue) {
            shorterTail = &global_network_status.server_list[block_type][i];
        }
    }
    return shorterTail;
}

void process_arrival() {
    blocks[TEMPERATURE_CTRL].total_arrivals++;
    blocks[TEMPERATURE_CTRL].jobInBlock++;

    server *s = findShorterServer(blocks[TEMPERATURE_CTRL]);

    // Se il server trovato non ha nessuno in servizio
    if (s->status == IDLE) {
        double serviceTime = getService(TEMPERATURE_CTRL, s->stream);
        compl c = {s, INFINITY};
        c.value = clock.current + serviceTime;
        s->status = BUSY;  // Setto stato busy
        s->sum.service += serviceTime;
        s->block->area.service += serviceTime;
        s->sum.served++;
        insertSorted(&global_sorted_completions, c);
        enqueue(s, clock.arrival);  // lo appendo nella linked list di job del blocco TEMP
    } else {
        enqueue(s, clock.arrival);  // lo appendo nella linked list di job del blocco TEMP
        s->jobInQueue++;
    }
    clock.arrival = getArrival(clock.current);  // Genera prossimo arrivo
}

void process_completion(compl c) {
    int block_type = c.server->block->type;
    blocks[block_type].total_completions++;
    blocks[block_type].jobInBlock--;

    int destination;
    server *shorterServer;

    dequeue(c.server);  // Toglie il job servito dal server e fa "avanzare" la lista collegata di job
    deleteElement(&global_sorted_completions, c);

    // Se nel server ci sono job in coda, devo generare il prossimo completamento per tale server.
    if (c.server->jobInQueue > 0 /*&&!c.server->need_resched */) {
        c.server->jobInQueue--;
        double service_1 = getService(block_type, c.server->stream);
        c.value = clock.current + service_1;
        c.server->sum.service += service_1;
        c.server->sum.served++;
        c.server->block->area.service += service_1;
        insertSorted(&global_sorted_completions, c);

    } else {
        c.server->status = IDLE;
    }

    // Se un server è schedulato per la terminazione, non prende un job dalla coda e và OFFLINE
    if (c.server->need_resched) {
        c.server->online = OFFLINE;
        c.server->time_online += (clock.current - c.server->last_online);
        c.server->last_online = clock.current;
        c.server->need_resched = false;
    }

    // Se il completamento avviene sul blocco GREEN PASS allora il job esce dal sistema
    if (block_type == GREEN_PASS) {
        completed++;
        return;
    }

    // Gestione blocco destinazione
    destination = getDestination(c.server->block->type);  // Trova la destinazione adatta per il job appena servito
    if (destination == EXIT) {
        dropped++;
        return;
    }
    if (destination != GREEN_PASS) {
        blocks[destination].total_arrivals++;
        blocks[destination].jobInBlock++;

        shorterServer = findShorterServer(blocks[destination]);
        enqueue(shorterServer, c.value);  // Posiziono il job nella coda del blocco destinazione e gli imposto come tempo di arrivo quello di completamento

        // Se il server trovato non ha nessuno in coda, generiamo un tempo di servizio
        if (shorterServer->jobInQueue == 0) {
            compl c2 = {shorterServer, INFINITY};
            double service_2 = getService(destination, shorterServer->stream);
            c2.value = clock.current + service_2;
            insertSorted(&global_sorted_completions, c2);
            shorterServer->status = BUSY;
            shorterServer->sum.service += service_2;
            shorterServer->sum.served++;
            shorterServer->block->area.service += service_2;

            return;
        } else {
            shorterServer->jobInQueue++;
            return;
        }
    }

    // Desination == GREEN_PASS. Se non ci sono serventi liberi il job esce dal sistema (loss system)
    blocks[destination].total_arrivals++;
    shorterServer = findShorterServer(blocks[destination]);
    if (shorterServer != NULL) {
        blocks[destination].jobInBlock++;
        // enqueue(&blocks[destination], c.value);  // Posiziono il job nella coda del blocco destinazione e gli imposto come tempo di arrivo quello di completamento

        compl c3 = {shorterServer, INFINITY};
        double service_3 = getService(destination, shorterServer->stream);
        c3.value = clock.current + service_3;
        insertSorted(&global_sorted_completions, c3);
        shorterServer->status = BUSY;
        shorterServer->sum.service += service_3;
        shorterServer->sum.served++;
        shorterServer->block->area.service += service_3;
        return;

    } else {
        completed++;
        bypassed++;
        blocks[GREEN_PASS].total_bypassed++;
        return;
    }
}

void init_config() {
    int slot_test[] = {3, 5, 7, 8, 6};
    config = get_config(slot_test, slot_test, slot_test);
}

int main() {
    PlantSeeds(521312312);
    init_config();
    init_network();
    int n = 1;
    while (clock.arrival <= stop_time) {
        set_time_slot();
        compl *nextCompletion = &global_sorted_completions.sorted_list[0];
        server *nextCompletionServer = nextCompletion->server;
        clock.next = min(nextCompletion->value, clock.arrival);  // Ottengo il prossimo evento

        // for (int i = 0; i < NUM_BLOCKS; i++) {
        //     if (blocks[i].jobInBlock > 0) {
        //         blocks[i].area.node += (clock.next - clock.current) * blocks[i].jobInBlock;
        //         blocks[i].area.queue += (clock.next - clock.current) * blocks[i].jobInQueue;
        //     }
        // }
        clock.current = clock.next;  // Avanzamento del clock al valore del prossimo evento

        if (clock.current == clock.arrival) {
            process_arrival();
        } else {
            process_completion(*nextCompletion);
        }
    }
}