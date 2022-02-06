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

// Function Prototypes
// --------------------------------------------------------
double getArrival(double current);
double getService(enum block_types type, int stream);
void process_arrival();
void process_completion(compl completion);
void init_blocks();
void activate_servers();
void deactivate_servers();
void update_network();
void init_config();
void enqueue_balancing(server *s, struct job *j);
void run();
void finite_horizon_simulation(int stop_time, int repetitions);
void finite_horizon_run(int stop, int repetition);
void end_servers();
void clear_environment();
void write_rt_csv_finite();
void init_config();
void print_results_finite();
void init_network(int rep);
void set_time_slot(int rep);
// ---------------------------------------------------------

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

int stop_simulation = TIME_SLOT_1 + TIME_SLOT_2 + TIME_SLOT_3;
// double stop_time = TIME_SLOT_1;
// double stop_time = TIME_SLOT_1 + TIME_SLOT_2;
int streamID;
int num_slot;
char *simulation_mode;
FILE *finite_csv;

double repetitions_costs[NUM_REPETITIONS];
double response_times[] = {0, 0, 0};
double statistics[NUM_REPETITIONS][3];
double global_means_p[BATCH_K][NUM_BLOCKS];
double global_means_p_fin[NUM_REPETITIONS][3][NUM_BLOCKS];

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: ./migliorativo <FINITE/INFINITE/TEST> <TIME_SLOT>\n");
        exit(0);
    }
    simulation_mode = argv[1];
    num_slot = atoi(argv[2]);

    // if (num_slot > 2) {
    //     printf("Specify time slot between 0 and 2\n");
    //     exit(0);
    // }
    if (str_compare(simulation_mode, "FINITE") == 0) {
        PlantSeeds(521312312);
        finite_horizon_simulation(stop_simulation, NUM_REPETITIONS);

    } else if (str_compare(simulation_mode, "INFINITE") == 0) {
        // PlantSeeds(521312312);
        // infinite_horizon_simulation(num_slot);

    } else {
        printf("Specify mode FINITE/INFINITE or TEST\n");
        exit(0);
    }
}

void finite_horizon_simulation(int stop_time, int repetitions) {
    printf("\n\n==== Finite Horizon Simulation | sim_time %d | #repetitions #%d ====", stop_simulation, NUM_REPETITIONS);
    init_config();
    print_configuration(&config);

    char filename[21];
    snprintf(filename, 21, "continuos_finite.csv");

    finite_csv = open_csv(filename);

    for (int r = 0; r < repetitions; r++) {
        finite_horizon_run(stop_time, r);
        print_percentage(r, repetitions, r - 1);
    }
    fclose(finite_csv);
    write_rt_csv_finite();
    print_results_finite();
}

void finite_horizon_run(int stop_time, int repetition) {
    init_network(0);
    int n = 1;
    double realSlotEnd;
    int settedRealTimeEnd = 0;

    while (clock.arrival <= stop_time) {
        set_time_slot(repetition);
        compl *nextCompletion = &global_sorted_completions.sorted_list[0];
        server *nextCompletionServer = nextCompletion->server;
        // if (!settedRealTimeEnd) {
        //     realSlotEnd = clock.current;
        //     settedRealTimeEnd = 1;
        // }
        clock.next = min(nextCompletion->value, clock.arrival);
        for (int i = 0; i < NUM_BLOCKS; i++) {
            for (int j = 0; j < MAX_SERVERS; j++) {  // Non posso fare il ciclo su num_online_servers altrimenti non aggiorno le statistiche di quelli con need_resched
                server *s = &global_network_status.server_list[i][j];

                if (s->jobInTotal > 0 && s->used) {
                    s->area.node += (clock.next - clock.current) * s->jobInTotal;
                    s->area.queue += (clock.next - clock.current) * s->jobInQueue;
                    s->area.service += (clock.next - clock.current);
                }
            }
        }
        clock.current = clock.next;  // Avanzamento del clock al valore del prossimo evento

        if (clock.current == clock.arrival) {
            process_arrival();
        } else {
            process_completion(*nextCompletion);
        }
        if (clock.current >= (n - 1) * 300 && clock.current < (n)*300 && completed > 16 && clock.arrival < stop_time) {
            calculate_statistics_clock(&global_network_status, blocks, clock.current, finite_csv);
            n++;
        }
    }
    end_servers();
    repetitions_costs[repetition] = calculate_cost(&global_network_status);
    calculate_statistics_fin(&global_network_status, clock.current, response_times, global_means_p_fin, repetition);
    for (int i = 0; i < 3; i++) {
        statistics[repetition][i] = response_times[i];
    }
    clear_environment();
}

void clear_environment() {
    global_sorted_completions = empty_sorted;
    global_network_status = empty_network;

    // TODO vedere se puo andare in init blocks, forse no perchè non vanno resettate le cose tra le batch.
    for (int block_type = 0; block_type < NUM_BLOCKS; block_type++) {
        for (int j = 0; j < MAX_SERVERS; j++) {
            if (global_network_status.server_list[block_type][j].used) {
                global_network_status.server_list[block_type][j].area.service = 0;
                global_network_status.server_list[block_type][j].area.node = 0;
                global_network_status.server_list[block_type][j].area.queue = 0;
            }
        }
    }
}

void end_servers() {
    for (int j = 0; j < NUM_BLOCKS; j++) {
        for (int i = 0; i < MAX_SERVERS; i++) {
            server *s = &global_network_status.server_list[j][i];
            if (s->online == ONLINE) {
                s->time_online += (clock.current - s->last_online);
                s->last_online = clock.current;
            }
        }
    }
}

double getArrival(double current) {
    double arrival = current;
    SelectStream(254);
    arrival += Exponential(1 / arrival_rate);
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
            s.jobInTotal = 0;
            s.completions = 0;
            s.arrivals = 0;

            //TODO AGGIUNTI PER MIGLIORATIVO
            s.head_service = NULL;
            s.tail = NULL;
            s.area.node = 0;
            s.area.service = 0;
            s.area.queue = 0;

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
        global_network_status.num_online_servers[block] = config.slot_config[slot][block];
    }
}

void load_balance(int block) {
    int slot = global_network_status.time_slot;
    int total_old = config.slot_config[slot - 1][block];
    int total_new = config.slot_config[slot][block];
    int total_job = blocks[block].jobInQueue;

    if (total_job == 0) {  // Non ci sono job da ri-distribuire
        return;
    }

    int only_new = total_new - total_old;
    int jobRemain = total_job / total_new;
    server last_server;

    int j = 0;
    int destID = 0;
    int lastID = 0;
    for (int i = 0; i < total_old; i++) {
        server *source = &global_network_status.server_list[block][i];
        while (source->jobInQueue > jobRemain) {
            destID = ((lastID) % (only_new)) + total_old;
            server *destination = &global_network_status.server_list[block][destID];

            struct job *tmp = source->tail->prev;
            enqueue_balancing(destination, source->tail);  // Mette la coda nella destinazione
            source->tail = tmp;
            if (source->tail->next) {
                source->tail->next = NULL;
            }

            destination->jobInTotal++;
            destination->arrivals++;
            if (destination->status == IDLE) {
                double serviceTime = getService(block, destination->stream);
                compl c = {destination, INFINITY};
                c.value = clock.current + serviceTime;
                destination->status = BUSY;
                insertSorted(&global_sorted_completions, c);
                blocks[block].jobInQueue--;  // Il primo job che andrà nel server IDLE APPENA ACCESSO non dovrà essere contato più come in coda, è in servizio
            } else {
                destination->jobInQueue++;  // Non dobbiamo aumentare anche block.jobInQueue perchè quel job è già contato come in coda
            }
            source->jobInQueue--;
            source->jobInTotal--;
            source->arrivals--;
            lastID++;
            // printf("s: ");
            // print_single_server_info(*source);
            // printf("d: ");
            // print_single_server_info(*destination);
        }
    }
}

void update_network() {
    int actual, new = 0;
    int slot = global_network_status.time_slot;
    // printf("\n==== BEFORE BALANCING SLOT %d ====", slot);
    // print_network_status(&global_network_status);
    for (int j = 0; j < NUM_BLOCKS; j++) {
        actual = global_network_status.num_online_servers[j];
        new = config.slot_config[slot][j];
        if (actual > new) {
            deactivate_servers(j);
        } else if (actual < new) {
            activate_servers(j);
            if (slot != 0)
                load_balance(j);
        }
    }
    // printf("\n==== AFTER BALANCING SLOT %d ====", slot);
    // print_network_status(&global_network_status);
}

void set_time_slot(int rep) {
    if (clock.current == START && !slot_switched[0]) {
        global_network_status.time_slot = 0;
        arrival_rate = LAMBDA_1;
        slot_switched[0] = true;
        update_network();
    }
    if (clock.current >= TIME_SLOT_1 && clock.current < TIME_SLOT_1 + TIME_SLOT_2 && !slot_switched[1]) {
        calculate_statistics_fin(&global_network_status, clock.current, response_times, global_means_p_fin, rep);

        global_network_status.time_slot = 1;
        arrival_rate = LAMBDA_2;
        slot_switched[1] = true;
        update_network();
    }

    if (clock.current >= TIME_SLOT_1 + TIME_SLOT_2 && !slot_switched[2]) {
        calculate_statistics_fin(&global_network_status, clock.current, response_times, global_means_p_fin, rep);

        global_network_status.time_slot = 2;
        arrival_rate = LAMBDA_3;
        slot_switched[2] = true;
        update_network();
    }
}

void init_network(int rep) {
    streamID = 0;
    clock.current = START;
    for (int i = 0; i < 3; i++) {
        slot_switched[i] = false;
        response_times[i] = 0;
    }

    init_blocks();
    if (str_compare(simulation_mode, "FINITE") == 0) {
        set_time_slot(rep);
    }

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

    // Appendi alla coda se esiste, altrimenti è la testa
    if (s->tail) {
        j->prev = s->tail;
        s->tail->next = j;  // aggiorno la vecchia coda e la faccio puntare a J
    } else {
        j->prev = NULL;
        s->head_service = j;
    }

    s->tail = j;  // J diventa la nuova coda
}

void enqueue_balancing(server *s, struct job *j) {
    if (s->tail != NULL) {  // Appendi alla coda se esiste, altrimenti è la testa
        s->tail->next = j;
        j->prev = s->tail;
    } else {
        s->head_service = j;
        if (s->head_service->prev)
            s->head_service->prev = NULL;
        if (s->head_service->next)
            s->head_service->next = NULL;
    }

    s->tail = j;
}

// Rimuove il job dalla coda del server specificato
void dequeue(server *s) {
    if (s->block->type == GREEN_PASS)
        return;

    struct job *j = s->head_service;

    if (!j->next)
        s->tail = NULL;

    s->head_service = j->next;
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
        }
        return NULL;
    }

    for (int i = 0; i < active_servers; i++) {
        server s = global_network_status.server_list[block_type][i];
        if (s.jobInTotal < shorterTail->jobInTotal && !s.need_resched) {
            shorterTail = &global_network_status.server_list[block_type][i];
        }
    }
    return shorterTail;
}

void process_arrival() {
    blocks[TEMPERATURE_CTRL].total_arrivals++;

    server *s = findShorterServer(blocks[TEMPERATURE_CTRL]);
    s->jobInTotal++;
    s->arrivals++;

    // Se il server trovato non ha nessuno in servizio, può servire il job appena arrivato
    if (s->status == IDLE) {
        double serviceTime = getService(TEMPERATURE_CTRL, s->stream);
        compl c = {s, INFINITY};
        c.value = clock.current + serviceTime;
        s->status = BUSY;  // Setto stato busy
        s->sum.service += serviceTime;
        // s->area.service += serviceTime;
        s->sum.served++;
        insertSorted(&global_sorted_completions, c);
        enqueue(s, clock.arrival);  // lo appendo nella linked list di job del blocco TEMP
    } else {
        enqueue(s, clock.arrival);  // lo appendo nella linked list di job del blocco TEMP
        s->jobInQueue++;
        blocks[TEMPERATURE_CTRL].jobInQueue++;
    }

    clock.arrival = getArrival(clock.current);  // Genera prossimo arrivo
}

void process_completion(compl c) {
    int block_type = c.server->block->type;

    blocks[block_type].total_completions++;
    c.server->completions++;
    c.server->jobInTotal--;

    int destination;
    server *shorterServer;

    dequeue(c.server);  // Toglie il job servito dal server e fa "avanzare" la lista collegata di job
    deleteElement(&global_sorted_completions, c);

    // Se nel server ci sono job in coda, devo generare il prossimo completamento per tale server.
    if (c.server->jobInQueue > 0) {
        c.server->jobInQueue--;
        blocks[block_type].jobInQueue--;
        double service_1 = getService(block_type, c.server->stream);
        c.value = clock.current + service_1;
        c.server->sum.service += service_1;
        c.server->sum.served++;
        // c.server->area.service += service_1;
        insertSorted(&global_sorted_completions, c);

    } else {
        c.server->status = IDLE;
    }

    // Se un server è schedulato per la terminazione e non ha job in coda va offline
    if (c.server->need_resched && c.server->jobInQueue == 0) {
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
        blocks[TEMPERATURE_CTRL].total_arrivals--;
        return;
    }
    if (destination != GREEN_PASS) {
        blocks[destination].total_arrivals++;

        shorterServer = findShorterServer(blocks[destination]);
        shorterServer->arrivals++;
        shorterServer->jobInTotal++;
        enqueue(shorterServer, c.value);  // Posiziono il job nella coda del blocco destinazione e gli imposto come tempo di arrivo quello di completamento

        // Se il server trovato non ha nessuno in coda, generiamo un tempo di servizio
        if (shorterServer->status == IDLE) {
            compl c2 = {shorterServer, INFINITY};
            double service_2 = getService(destination, shorterServer->stream);
            c2.value = clock.current + service_2;
            insertSorted(&global_sorted_completions, c2);
            shorterServer->status = BUSY;
            shorterServer->sum.service += service_2;
            shorterServer->sum.served++;
            // shorterServer->area.service += service_2;
            return;
        } else {
            shorterServer->jobInQueue++;
            blocks[destination].jobInQueue++;
            return;
        }
    }

    // Desination == GREEN_PASS. Se non ci sono serventi liberi il job esce dal sistema (loss system)
    blocks[destination].total_arrivals++;
    shorterServer = findShorterServer(blocks[destination]);

    if (shorterServer != NULL) {
        shorterServer->jobInTotal++;
        shorterServer->arrivals++;
        compl c3 = {shorterServer, INFINITY};
        double service_3 = getService(destination, shorterServer->stream);
        c3.value = clock.current + service_3;
        insertSorted(&global_sorted_completions, c3);
        shorterServer->status = BUSY;
        shorterServer->sum.service += service_3;
        shorterServer->sum.served++;
        // shorterServer->area.service += service_3;
        return;

    } else {
        completed++;
        bypassed++;
        blocks[GREEN_PASS].total_bypassed++;
        return;
    }
}

void init_config() {
    int slot_null[] = {0, 0, 0, 0, 0};
    int slot_test_1[] = {7, 20, 2, 9, 11};
    int slot_test_2[] = {1, 1, 1, 1, 1};
    int slot_test_3[] = {14, 40, 3, 18, 20};
    int slot_test_4[] = {6, 18, 2, 10, 10};
    int slot_test_5[] = {40, 40, 40, 40, 40};
    // config = get_config(slot_test_1, slot_null, slot_null);
    config = get_config(slot_test_1, slot_test_3, slot_test_4);
}

/*
void run() {
    init_config();
    init_network();
    int n = 1;
    double realSlotEnd;
    int settedRealTimeEnd = 0;

    while (clock.arrival <= stop_simulation + 0) {
        set_time_slot();
        compl *nextCompletion = &global_sorted_completions.sorted_list[0];
        server *nextCompletionServer = nextCompletion->server;
        if (clock.current > stop_simulation) {
            if (!settedRealTimeEnd) {
                realSlotEnd = clock.current;
                settedRealTimeEnd = 1;
            }
            clock.next = nextCompletion->value;
            if (clock.next == INFINITY) {
                break;
            }
        } else {
            clock.next = min(nextCompletion->value, clock.arrival);  // Ottengo il prossimo evento
        }

        for (int i = 0; i < NUM_BLOCKS; i++) {
            for (int j = 0; j < MAX_SERVERS; j++) {  // Non posso fare il ciclo su num_online_servers altrimenti non aggiorno le statistiche di quelli con need_resched
                server *s = &global_network_status.server_list[i][j];

                if (s->jobInTotal > 0 && s->used) {
                    s->area.node += (clock.next - clock.current) * s->jobInTotal;
                    s->area.queue += (clock.next - clock.current) * s->jobInQueue;
                    s->area.service += (clock.next - clock.current);
                }
            }
        }
        clock.current = clock.next;  // Avanzamento del clock al valore del prossimo evento

        if (clock.current == clock.arrival) {
            process_arrival();
        } else {
            process_completion(*nextCompletion);
        }
    }

    print_configuration(&config);
    print_network_status(&global_network_status);
    printf("Arrivi generati che non vengono rifiutati: %d\n", blocks[TEMPERATURE_CTRL].total_arrivals);
    printf("Escono dal sistema: %d serviti da green pass e %d bypassati\n", blocks[GREEN_PASS].total_completions, blocks[GREEN_PASS].total_bypassed);
    printf("Dropped :%d\n", dropped);

    int stillInSystem = 0;
    for (int j = 0; j < NUM_BLOCKS; j++) {
        for (int i = 0; i < MAX_SERVERS; i++) {
            server s = global_network_status.server_list[j][i];
            if (s.used == NOTUSED) {
                break;
            }
            stillInSystem += s.jobInTotal;
        }
    }
    printf("Job ancora nel sistema :%d\n", stillInSystem);
    int total = blocks[GREEN_PASS].total_bypassed + blocks[GREEN_PASS].total_completions + stillInSystem;  // Dropped sono già tolti dagli arrivati alla temperatura
    printf("\nVerifica:\n\t + %d (completamenti) \n\t + %d (bypassed) \n\t + %d (stillInSystem)\n\t ========\n\t  %d {arrivi generati: %d}\n", blocks[GREEN_PASS].total_completions, blocks[GREEN_PASS].total_bypassed, stillInSystem, total, blocks[TEMPERATURE_CTRL].total_arrivals);

    print_servers_statistics(&global_network_status, realSlotEnd, clock.current);
}
*/

void write_rt_csv_finite() {
    FILE *csv;
    char filename[30];
    for (int j = 0; j < 3; j++) {
        snprintf(filename, 30, "rt_finite_slot%d.csv", j);
        csv = open_csv(filename);
        for (int i = 0; i < NUM_REPETITIONS; i++) {
            append_on_csv(csv, i, statistics[i][j], 0);
        }
        fclose(csv);
    }
}

void print_results_finite() {
    double total = 0;
    for (int i = 0; i < NUM_REPETITIONS; i++) {
        total += repetitions_costs[i];
    }

    printf("\nTOTAL MEAN CONFIGURATION COST: %f\n", total / NUM_REPETITIONS);
    for (int s = 0; s < 3; s++) {
        printf("\nSlot #%d:", s);
        for (int j = 0; j < NUM_BLOCKS; j++) {
            printf("\nMean Utilization for block %s: ", stringFromEnum(j));
            double p = 0;
            for (int i = 0; i < NUM_REPETITIONS; i++) {
                p += global_means_p_fin[i][s][j];
            }
            printf("%f", p / NUM_REPETITIONS);
        }
    }
}