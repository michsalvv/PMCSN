#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "./DES/rngs.h"
#include "./DES/rvgs.h"
#include "config.h"
#include "math.h"
#include "utils.h"

// Function Prototypes
// ------------------------------------------------------------------------------------------------
double getArrival(double current);
void enqueue(struct block *block, double arrival);
void dequeue(struct block *block);
server *findFreeServer(struct block b);
double findNextEvent(double nextArrival, struct block *services, server **server_completion);
double getService(enum block_types type, int stream);
void process_arrival();
void process_completion(compl completion);
void init_network(int rep);
void init_blocks();
void set_time_slot(int rep);
void activate_servers();
void deactivate_servers();
void update_network();
void finite_horizon_simulation(int stop_time, int repetitions);
void infinite_horizon_simulation(int num_slot);
void finite_horizon_run(int stop, int repetition);
void infinite_horizon_batch(int slot, int b, int k);
void end_servers();
void clear_environment();
void write_rt_csv_finite();
void write_rt_csv_infinite(int slot);
void init_config();
void find_batch_b(int slot);
void reset_statistics();
void print_results_finite();
void print_results_infinite(int slot);
void print_ploss();
// ------------------------------------------------------------------------------------------------
network_configuration config;
sorted_completions global_sorted_completions;  // Tiene in una lista ordinata tutti i completamenti nella rete così da ottenere il prossimo in O(log(N))
network_status global_network_status;          // Tiene lo stato complessivo della rete
struct block blocks[NUM_BLOCKS];               // Mantiene lo stato dei singoli blocchi della rete
struct clock_t clock;                          // Mantiene le informazioni sul clock di simulazione
static const sorted_completions empty_sorted;
static const network_status empty_network;

double arrival_rate;
double lambdas[] = {LAMBDA_1, LAMBDA_2, LAMBDA_3};
int stop_simulation = TIME_SLOT_1 + TIME_SLOT_2 + TIME_SLOT_3;
int completed;
int dropped;
int bypassed;
bool slot_switched[3];

int streamID;            // Stream da selezionare per generare il tempo di servizio
server *nextCompletion;  // Tiene traccia del server relativo al completamento imminente

char *simulation_mode;
int num_slot;

double statistics[NUM_REPETITIONS][3];
double infinite_statistics[BATCH_K];
double infinite_delay[BATCH_K][NUM_BLOCKS];
double repetitions_costs[NUM_REPETITIONS];
double global_means_p[BATCH_K][NUM_BLOCKS];
double global_means_p_fin[NUM_REPETITIONS][3][NUM_BLOCKS];
double global_loss[BATCH_K];
// -------------------------------------------------------------------------------------------------

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: ./simulate-base <FINITE/INFINITE> <TIME_SLOT>\n");
        exit(0);
    }
    simulation_mode = argv[1];
    num_slot = atoi(argv[2]);

    if (num_slot > 2) {
        printf("Specify time slot between 0 and 2\n");
        exit(0);
    }
    if (str_compare(simulation_mode, "FINITE") == 0) {
        PlantSeeds(231232132);
        remove("results/finite/continuos_finite.csv");
        finite_horizon_simulation(stop_simulation, NUM_REPETITIONS);

    } else if (str_compare(simulation_mode, "INFINITE") == 0) {
        PlantSeeds(231232132);
        infinite_horizon_simulation(num_slot);

    } else {
        printf("Specify mode FINITE or INFINITE\n");
        exit(0);
    }
}

// Esegue le ripetizioni di singole run a orizzonte finito
void finite_horizon_simulation(int stop_time, int repetitions) {
    printf("\n\n==== Finite Horizon Simulation | sim_time %d | #repetitions #%d ====", stop_simulation, NUM_REPETITIONS);
    init_config();
    print_configuration(&config);
    print_line();
    for (int r = 0; r < repetitions; r++) {
        finite_horizon_run(stop_time, r);
        if (r == 0 && strcmp(simulation_mode, "FINITE") == 0) {
            print_p_on_csv(&global_network_status, clock.current, 2);
        }
        clear_environment();
        print_percentage(r, repetitions, r - 1);
    }
    print_line();
    write_rt_csv_finite();
    print_results_finite();
}

// Esegue una simulazione ad orizzonte infinito tramite il metodo delle batch means
void infinite_horizon_simulation(int slot) {
    printf("\n\n==== Infinite Horizon Simulation for slot %d | #batch %d====", slot, BATCH_K);
    init_config();
    print_configuration(&config);
    arrival_rate = lambdas[slot];
    int b = BATCH_B;
    clear_environment();
    init_network(0);
    global_network_status.time_slot = slot;
    update_network();
    for (int k = 0; k < BATCH_K; k++) {
        infinite_horizon_batch(slot, b, k);
        reset_statistics();
        print_percentage(k, BATCH_K, k - 1);
    }
    write_rt_csv_infinite(slot);
    end_servers();
    print_results_infinite(slot);
}

// Esegue diverse run di batch mean con diversi valori di b
void find_batch_b(int slot) {
    arrival_rate = lambdas[slot];
    int b = 64;
    for (b; b < 2058; b = b * 2) {
        PlantSeeds(521312312);
        clear_environment();
        init_config();
        init_network(0);
        update_network();
        for (int k = 0; k < 128; k++) {
            infinite_horizon_batch(slot, b, k);
        }
        char filename[100];
        snprintf(filename, 100, "/results/infinite/rt_batch_inf_%d.csv", b);
        FILE *csv;
        csv = open_csv(filename);
        for (int j = 0; j < 128; j++) {
            append_on_csv(csv, j, infinite_statistics[j], 0);
        }
        fclose(csv);
    }
}

// Esegue una singola run di simulazione ad orizzonte finito
void finite_horizon_run(int stop_time, int repetition) {
    init_network(0);
    int n = 1;
    while (clock.arrival <= stop_time) {
        set_time_slot(repetition);
        compl *nextCompletion = &global_sorted_completions.sorted_list[0];
        server *nextCompletionServer = nextCompletion->server;
        clock.next = min(nextCompletion->value, clock.arrival);  // Ottengo il prossimo evento

        for (int i = 0; i < NUM_BLOCKS; i++) {
            if (blocks[i].jobInBlock > 0) {
                blocks[i].area.node += (clock.next - clock.current) * blocks[i].jobInBlock;
                blocks[i].area.queue += (clock.next - clock.current) * blocks[i].jobInQueue;
            }
        }
        clock.current = clock.next;  // Avanzamento del clock al valore del prossimo evento

        if (clock.current == clock.arrival) {
            process_arrival();
        } else {
            process_completion(*nextCompletion);
        }
        if (clock.current >= (n - 1) * 300 && clock.current < (n)*300 && completed > 16 && clock.arrival < stop_time) {
            calculate_statistics_clock(&global_network_status, blocks, clock.current);
            n++;
        }
    }
    calculate_statistics_fin(&global_network_status, blocks, clock.current, statistics, repetition);
    end_servers();
    repetitions_costs[repetition] = calculate_cost(&global_network_status);
}

// Esegue un singolo batch ad orizzonte infinito
void infinite_horizon_batch(int slot, int b, int k) {
    int n = 0;
    int q = 0;
    global_network_status.time_slot = slot;
    double old;
    while (n < b || q < b) {
        compl *nextCompletion = &global_sorted_completions.sorted_list[0];
        server *nextCompletionServer = nextCompletion->server;
        if (n >= b) {
            clock.next = nextCompletion->value;  // Ottengo il prossimo evento
            if (clock.next == INFINITY) {
                break;
            }
        } else {
            clock.next = min(nextCompletion->value, clock.arrival);  // Ottengo il prossimo evento
        }

        for (int i = 0; i < NUM_BLOCKS; i++) {
            if (blocks[i].jobInBlock > 0) {
                blocks[i].area.node += (clock.next - clock.current) * blocks[i].jobInBlock;
                blocks[i].area.queue += (clock.next - clock.current) * blocks[i].jobInQueue;
            }
        }
        clock.current = clock.next;  // Avanzamento del clock al valore del prossimo evento
        if (clock.current == clock.arrival) {
            process_arrival();
            n++;

        } else {
            process_completion(*nextCompletion);
            q++;
        }
    }
    calculate_statistics_inf(&global_network_status, blocks, (clock.current - clock.batch_current), infinite_statistics, k, infinite_delay);

    for (int i = 0; i < NUM_BLOCKS; i++) {
        double p = 0;
        int n = 0;
        for (int j = 0; j < MAX_SERVERS; j++) {
            server s = global_network_status.server_list[i][j];
            if (s.used == 1) {
                p += (s.sum.service / clock.current);
                n++;
            }
        }
        if (i == GREEN_PASS) {
            double loss_perc = (float)blocks[i].total_bypassed / (float)blocks[i].total_arrivals;
            global_loss[k] = loss_perc;
        }
        global_means_p[k][i] = p / n;
    }
}

// Processa un arrivo dall'esterno verso il sistema
void process_arrival() {
    blocks[TEMPERATURE_CTRL].total_arrivals++;
    blocks[TEMPERATURE_CTRL].jobInBlock++;

    server *s = findFreeServer(blocks[TEMPERATURE_CTRL]);

    // C'è un servente libero, quindi genero il completamento
    if (s != NULL) {
        double serviceTime = getService(TEMPERATURE_CTRL, s->stream);
        compl c = {s, INFINITY};
        c.value = clock.current + serviceTime;
        s->status = BUSY;  // Setto stato busy
        s->sum.service += serviceTime;
        s->block->area.service += serviceTime;
        s->sum.served++;
        insertSorted(&global_sorted_completions, c);
        enqueue(&blocks[TEMPERATURE_CTRL], clock.arrival);  // lo appendo nella linked list di job del blocco TEMP
    } else {
        enqueue(&blocks[TEMPERATURE_CTRL], clock.arrival);  // lo appendo nella linked list di job del blocco TEMP
        blocks[TEMPERATURE_CTRL].jobInQueue++;              // Se non c'è un servente libero aumenta il numero di job in coda
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

    dequeue(&blocks[block_type]);  // Toglie il job servito dal blocco e fa "avanzare" la lista collegata di job
    deleteElement(&global_sorted_completions, c);

    // Se nel blocco ci sono job in coda, devo generare il prossimo completamento per il servente che si è liberato.
    if (blocks[block_type].jobInQueue > 0 && !c.server->need_resched) {
        blocks[block_type].jobInQueue--;
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
        blocks[block_type].total_dropped++;
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

    // Desination == GREEN_PASS. Se non ci sono serventi liberi il job esce dal sistema (loss system)
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

// Genera un tempo di arrivo secondo la distribuzione Esponenziale
double getArrival(double current) {
    double arrival = current;
    SelectStream(254);
    arrival += Exponential(1 / arrival_rate);
    return arrival;
}

// Genera un tempo di servizio esponenziale di media specificata e stream del servente individuato
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

// Rimuove il job dalla coda del blocco specificata
void dequeue(struct block *block) {
    struct job *j = block->head_service;

    if (!j->next)
        block->tail = NULL;

    block->head_service = j->next;

    if (block->head_queue != NULL && block->head_queue->next != NULL) {
        struct job *tmp = block->head_queue->next;
        block->head_queue = tmp;
    } else {
        block->head_queue = NULL;
    }
    free(j);
}

// Ritorna il primo server libero nel blocco specificato
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

// Inizializza tutti i blocchi del sistema
void init_network(int rep) {
    global_network_status.configuration = &config;
    streamID = 0;
    clock.current = START;
    for (int i = 0; i < 3; i++) {
        slot_switched[i] = false;
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

// Inizializza tutti i serventi di tutti i blocchi della rete
void init_blocks() {
    for (int block_type = 0; block_type < NUM_BLOCKS; block_type++) {
        blocks[block_type].type = block_type;
        blocks[block_type].jobInBlock = 0;
        blocks[block_type].jobInQueue = 0;
        blocks[block_type].total_arrivals = 0;
        blocks[block_type].total_completions = 0;
        blocks[block_type].total_bypassed = 0;
        blocks[block_type].total_dropped = 0;
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
            global_network_status.server_list[block_type][i] = s;

            compl c = {&global_network_status.server_list[block_type][i], INFINITY};
            insertSorted(&global_sorted_completions, c);
        }
    }
}

// Cambia la fascia oraria settando il tasso di arrivo ed attivando/disattivando i server necessari
void set_time_slot(int rep) {
    if (clock.current == START) {
        global_network_status.time_slot = 0;
        arrival_rate = LAMBDA_1;
        slot_switched[0] = true;
        update_network();
    }
    if (clock.current >= TIME_SLOT_1 && clock.current < TIME_SLOT_1 + TIME_SLOT_2 && !slot_switched[1]) {
        calculate_statistics_fin(&global_network_status, blocks, clock.current, statistics, rep);

        if (rep == 0 && strcmp(simulation_mode, "FINITE") == 0) {
            print_p_on_csv(&global_network_status, clock.current, global_network_status.time_slot);
        }
        global_network_status.time_slot = 1;
        arrival_rate = LAMBDA_2;
        slot_switched[1] = true;
        update_network();
    }
    if (clock.current >= TIME_SLOT_1 + TIME_SLOT_2 && !slot_switched[2]) {
        calculate_statistics_fin(&global_network_status, blocks, clock.current, statistics, rep);
        if (rep == 0 && strcmp(simulation_mode, "FINITE") == 0) {
            print_p_on_csv(&global_network_status, clock.current, global_network_status.time_slot);
        }
        global_network_status.time_slot = 2;
        arrival_rate = LAMBDA_3;
        slot_switched[2] = true;
        update_network();
    }
}

void print_ploss() {
    double loss_perc = (float)blocks[GREEN_PASS].total_bypassed / (float)blocks[GREEN_PASS].total_arrivals;
    printf("P_LOSS: %f\n", loss_perc);
}

// Aggiorna i serventi attivi al cambio di fascia, attivando o disattivando il numero necessario per ogni blocco
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

// Attiva un certo numero di server per il blocco, fino al numero specificato dalla configurazione.
void activate_servers(int block) {
    int start = 0;
    int slot = global_network_status.time_slot;

    start = global_network_status.num_online_servers[block];
    for (int i = start; i < config.slot_config[slot][block]; i++) {
        server *s = &global_network_status.server_list[block][i];
        s->online = ONLINE;
        s->last_online = clock.current;
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

// Disattiva un certo numero di server per il blocco, fino al numero specificato dalla configurazione
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

// Calcola il tempo online per i server al termine della simulazione
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

// Resetta l'ambiente di esecuzione tra due run ad orizzonte finito
void clear_environment() {
    global_sorted_completions = empty_sorted;
    global_network_status = empty_network;

    for (int block_type = 0; block_type < NUM_BLOCKS; block_type++) {
        blocks[block_type].area.node = 0;
        blocks[block_type].area.service = 0;
        blocks[block_type].area.queue = 0;
    }
}

// Resetta le statistiche tra un batch ed il successivo
void reset_statistics() {
    clock.batch_current = clock.current;
    for (int block_type = 0; block_type < NUM_BLOCKS; block_type++) {
        blocks[block_type].total_arrivals = 0;
        blocks[block_type].total_completions = 0;
        blocks[block_type].total_bypassed = 0;
        blocks[block_type].total_dropped = 0;
        blocks[block_type].area.node = 0;
        blocks[block_type].area.service = 0;
        blocks[block_type].area.queue = 0;
    }
}

// Scrive i tempi di risposta a tempo infinito su un file csv
void write_rt_csv_infinite(int slot) {
    char filename[100];
    char filename_ploss[100];

    snprintf(filename, 100, "results/infinite/rt_infinite_slot_%d.csv", slot);
    snprintf(filename_ploss, 100, "results/infinite/ploss_infinite_slot_%d.csv", slot);
    FILE *csv;
    FILE *csv_ploss;
    csv = open_csv(filename);
    csv_ploss = open_csv(filename_ploss);

    for (int j = 0; j < BATCH_K; j++) {
        append_on_csv(csv, j, infinite_statistics[j], 0);
        append_on_csv(csv_ploss, j, global_loss[j], 0);
    }
    fclose(csv);
    fclose(csv_ploss);

    for (int i = 0; i < NUM_BLOCKS - 1; i++) {
        char filename_delays[100];
        snprintf(filename_delays, 100, "results/infinite/dl_%d_infinite_slot_%d.csv", i, slot);
        FILE *csv_delays;
        csv_delays = open_csv(filename_delays);

        for (int j = 0; j < BATCH_K; j++) {
            append_on_csv(csv_delays, j, infinite_delay[j][i], 0);
        }
        fclose(csv_delays);
    }
}

// Scrive i tempi di risposta a tempo finito su un file csv
void write_rt_csv_finite() {
    FILE *csv;
    char filename[100];
    for (int j = 0; j < 3; j++) {
        snprintf(filename, 100, "results/finite/rt_finite_slot%d.csv", j);
        csv = open_csv(filename);
        for (int i = 0; i < NUM_REPETITIONS; i++) {
            append_on_csv(csv, i, statistics[i][j], 0);
        }
        fclose(csv);
    }
}

// Stampa il costo e l'utilizzazione media ad orizzonte finito
void print_results_finite() {
    double total = 0;
    for (int i = 0; i < NUM_REPETITIONS; i++) {
        total += repetitions_costs[i];
    }
    printf("\nTOTAL MEAN CONFIGURATION COST: %f\n", total / NUM_REPETITIONS);
}

// Stampa il costo e l'utilizzazione media ad orizzonte infinito
void print_results_infinite(int slot) {
    double cost = calculate_cost(&global_network_status);
    printf("\n\nTOTAL SLOT %d CONFIGURATION COST: %f\n", slot, cost);

    double l = 0;
    for (int j = 0; j < NUM_BLOCKS; j++) {
        printf("\nMean Utilization for block %s: ", stringFromEnum(j));
        double p = 0;
        for (int i = 0; i < BATCH_K; i++) {
            p += global_means_p[i][j];
            if (j == GREEN_PASS) {
                l += global_loss[i];
            }
        }
        printf("%f", p / BATCH_K);
    }
    printf("\nGREEN PASS LOSS PERC %f: ", l / BATCH_K);
    printf("\n");
}

// Setta la configurazione di avvio specificata
void init_config() {
    int slot_null[] = {0, 0, 0, 0, 0};

    // Slot 0 Config 1 [infinita]
    int slot0_conf_1[] = {3, 20, 1, 5, 15};

    // Slot 0 Config 2 [non-ottima]
    int slot0_conf_2[] = {10, 30, 3, 20, 15};

    // Slot 0 Config 4 [infinita]
    int slot0_conf_4[] = {8, 24, 1, 11, 14};

    // Slot 0 Config 4_bis [non-ottima]
    int slot0_conf_4_bis[] = {8, 24, 2, 11, 14};

    // Slot 0 Config 5 [non-ottima]
    int slot0_conf_5[] = {9, 19, 3, 11, 15};

    // Slot 0 Config 5_bis [OTTIMO]
    int slot0_conf_5_bis[] = {7, 20, 2, 9, 11};

    // Slot 1 Config 1 [non-ottima]
    int slot1_conf_1[] = {18, 42, 5, 22, 25};

    // Slot 1 Connfig 2 [OTTIMO]
    int slot1_conf_2[] = {14, 40, 3, 17, 20};

    // Slot 1 Config 3 [infinita]
    int slot1_conf_3[] = {10, 30, 2, 12, 14};

    // Slot 2 Config 1 [non-ottima]
    int slot2_conf_1[] = {10, 30, 3, 12, 16};

    // Slot 2 Config 2 [OTTIMO]
    int slot2_conf_2[] = {7, 20, 2, 8, 10};

    // Slot 2 Config 3 [infinita]
    int slot2_conf_3[] = {4, 14, 1, 6, 8};

    // Slot 2 Config 4 [non-ottima e spropositata nei costi]
    int slot2_conf_4[] = {15, 45, 5, 18, 30};

    /* 
    * =================
    * SIMULAZIONE INFINITA
    * =================
    */

    // Configurazioni Infinite Horizon Slot 1
    // config = get_config(slot0_conf_1, slot_null, slot_null);
    // config = get_config(slot0_conf_2, slot_null, slot_null);
    // config = get_config(slot0_conf_4, slot_null, slot_null);
    // config = get_config(slot0_conf_4_bis, slot_null, slot_null);
    // config = get_config(slot0_conf_5, slot_null, slot_null);

    // Configurazioni Infinite Horizon Slot 1
    // config = get_config(slot_null, slot1_conf_1, slot_null);

    // Configurazioni Infinite Horizon Slot 2
    // config = get_config(slot_null, slot_null, slot2_conf_1);

    int slot0_ottima[] = {8, 21, 2, 9, 11};
    int slot1_ottima[] = {14, 41, 3, 17, 20};
    int slot2_ottima[] = {8, 18, 2, 9, 10};
    // config = get_config(slot0_ottima, slot1_ottima, slot2_ottima);

    /* 
    * =================
    * SIMULAZIONE FINITA
    * =================
    */

    // Scenario 1: prima non funzionante, resto non ottime
    // config = get_config(slot0_conf_1, slot1_conf_1, slot2_conf_1);

    // Scenario 2: tutte non-ottime {over-provisioning}
    // config = get_config(slot0_conf_2, slot1_conf_1, slot2_conf_1);

    // Scenario 3: under-provisioning
    // config = get_config(slot0_conf_1, slot1_conf_3, slot2_conf_3);

    // Scenario 4: configurazione ottima
    // config = get_config(slot0_ottima, slot1_ottima, slot2_ottima);

    // Scenario 5: configurazione ottima solo per fascia centrale, la più affollata
    // config = get_config(slot0_conf_5, slot1_conf_2, slot2_conf_1);

    // Scenario 6: ottima, non-ottima, infinita
    // config = get_config(slot0_ottima, slot1_conf_1, slot2_conf_3);

    // Scenario 7: non-ottima, infinita, non-ottima con molti server
    // config = get_config(slot0_conf_2, slot1_conf_3, slot0_conf_2);

    int s1[] = {9, 22, 3, 11, 10};
    int s2[] = {14, 42, 4, 20, 20};
    int s3[] = {9, 20, 3, 12, 10};

    // Configurazioni ottime algoritmo migliorativo, messe qui per confronto utilizzazioni
    int slot0_ottima_multiqueue[] = {8, 22, 2, 10, 11};
    int slot1_ottima_multiqueue[] = {14, 43, 3, 17, 20};
    int slot2_ottima_multiqueue[] = {8, 18, 2, 9, 10};
    // config = get_config(slot0_ottima_multiqueue, slot1_ottima_multiqueue, slot2_ottima_multiqueue);

    int a[] = {8, 20, 2, 9, 11};
    int b[] = {18, 42, 5, 22, 25};
    int c[] = {4, 14, 1, 6, 8};
    config = get_config(a, b, c);
}
