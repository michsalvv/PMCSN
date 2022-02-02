#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
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
void repeat_finite(int stop_time, int repetitions);

void finite_horizon_simulation(int stop, int repetition);
void infinite_horizon_simulation(int slot);
void end_servers();

bool slot_switched[3];
network_configuration config;

int streamID;                                  // Stream da selezionare per generare il tempo di servizio
server *nextCompletion;                        // Tiene traccia del server relativo al completamento imminente
sorted_completions global_sorted_completions;  // Tiene in una lista ordinata tutti i completamenti nella rete così da ottenere il prossimo in O(log(N))
network_status global_network_status;

static const sorted_completions empty_sorted;
static const network_status empty_network;

int completed = 0;
int dropped = 0;
int bypassed = 0;

struct block blocks[NUM_BLOCKS];
struct clock_t clock;

double arrival_rate;

int S1 = TIME_SLOT_1;
int S2 = TIME_SLOT_1 + TIME_SLOT_2;
int S3 = TIME_SLOT_1 + TIME_SLOT_2 + TIME_SLOT_3;

int infinites[] = {TIME_SLOT_1_INF, TIME_SLOT_2_INF, TIME_SLOT_3_INF};
double lambdas[] = {LAMBDA_1, LAMBDA_2, LAMBDA_3};

int stop_simulation = 0;
char *simulation_mode;
int num_slot;

double response_times[] = {0, 0, 0};
double statistics[NUM_REPETITIONS][3];

//TODO iniziare con il metodo della replicazione per lo stato finito. Metterer un parametro da argv che è il numero di run.
//TODO iniziare con il batch means per lo stato infinito. Vedere b e k come si dimensionano e togliere il alore TIME_SLOT*15 e mettere insomma i batch.

// Resetta l'ambiente di esecuzione tra due run ad orizzonte finito
void clear_environment() {
    global_sorted_completions = empty_sorted;
    global_network_status = empty_network;

    // TODO vedere se puo andare in init blocks, forse no perchè non vanno resettate le cose tra le batch.
    for (int block_type = 0; block_type < NUM_BLOCKS; block_type++) {
        blocks[block_type].area.node = 0;
        blocks[block_type].area.service = 0;
        blocks[block_type].area.queue = 0;
    }
}

void write_rt_on_csv() {
    FILE *csv;
    char filename[13];
    for (int j = 0; j < 3; j++) {
        snprintf(filename, 13, "rt_slot%d.csv", j);
        csv = open_csv(filename);
        for (int i = 0; i < NUM_REPETITIONS; i++) {
            append_on_csv(csv, i, statistics[i][j], 0);
        }
        fclose(csv);
    }
}

void init_config() {
    // Config 1
    int slot1_conf[] = {3, 20, 1, 5, 15};
    int slot2_conf[] = {5, 45, 4, 15, 25};
    int slot3_conf[] = {2, 10, 1, 3, 15};

    // Config_2
    int slot1_conf_2[] = {10, 30, 3, 20, 15};
    int slot2_conf_2[] = {15, 45, 4, 15, 20};
    int slot3_conf_2[] = {12, 20, 2, 40, 10};

    // Config_3
    int slot1_conf_3[] = {3, 27, 2, 10, 15};
    int slot2_conf_3[] = {5, 39, 3, 15, 25};
    int slot3_conf_3[] = {3, 21, 2, 10, 15};

    // Config_4
    int slot1_conf_4[] = {8, 24, 1, 11, 14};
    int slot2_conf_4[] = {14, 41, 3, 17, 20};
    int slot3_conf_4[] = {8, 20, 2, 9, 10};

    // Config_5
    int slot1_conf_5[] = {7, 20, 2, 9, 15};
    int slot2_conf_5[] = {14, 40, 3, 18, 20};
    int slot3_conf_5[] = {6, 18, 2, 8, 10};

    //config = get_config(slot1_conf, slot2_conf, slot3_conf);
    //config = get_config(slot1_conf_2, slot2_conf_2, slot3_conf_2);
    //config = get_config(slot1_conf_3, slot2_conf_3, slot3_conf_3);
    //config = get_config(slot1_conf_4, slot2_conf_4, slot3_conf_4);
    config = get_config(slot1_conf_5, slot2_conf_5, slot3_conf_5);
}

void print_configuration() {
    for (int slot = 0; slot < 3; slot++) {
        printf("FASCIA #%d\n", slot);
        for (int block = 0; block < NUM_BLOCKS; block++) {
            printf("...%s: %d\n", stringFromEnum(block), config.slot_config[slot][block]);
        }
        printf("\n");
    }
}

int main(int argc, char *argv[]) {
    PlantSeeds(521312312);
    if (argc != 3) {
        printf("Default Simultation\n");
        simulation_mode = "FINITE";
        stop_simulation = S3;
        finite_horizon_simulation(stop_simulation, 0);
        exit(0);
    }
    simulation_mode = argv[1];
    num_slot = atoi(argv[2]);

    switch (num_slot) {
        case 0:
            stop_simulation = S1;
            break;
        case 1:
            stop_simulation = S2;
            break;
        case 2:
            stop_simulation = S3;
            break;

        default:
            stop_simulation = S3;
    }
    if (str_compare(simulation_mode, "FINITE") == 0) {
        repeat_finite(stop_simulation, NUM_REPETITIONS);
        write_rt_on_csv();
    } else if (str_compare(simulation_mode, "INFINITE") == 0) {
        infinite_horizon_simulation(num_slot);
    }
}

void repeat_finite(int stop_time, int repetitions) {
    init_config();
    print_configuration();
    for (int r = 0; r < repetitions; r++) {
        finite_horizon_simulation(stop_time, r);
    }
}

void finite_horizon_simulation(int stop_time, int repetition) {
    printf("\n\n==== Finite Horizon Simulation | sim_time %d | repetition #%d ====", stop_time, repetition);
    print_line();
    init_network();
    double old = 0;
    while (clock.arrival <= stop_time) {
        print_percentage(clock.current, stop_time, old);
        old = clock.current;
        set_time_slot();
        compl *nextCompletion = &global_sorted_completions.sorted_list[0];
        server *nextCompletionServer = nextCompletion->server;
        clock.next = min(nextCompletion->value, clock.arrival);  // Ottengo il prossimo evento

        for (int i = 0; i < NUM_BLOCKS; i++) {
            if (blocks[i].jobInBlock > 0) {
                blocks[i].area.node += (clock.next - clock.current) * blocks[i].jobInBlock;
                blocks[i].area.queue += (clock.next - clock.current) * blocks[i].jobInQueue;
                //blocks[i].area.service += (clock.next - clock.current);  //TODO Da togliere?? Utilizzazione non si calcola cosi, va calcolata per ogni server. La mettiamo a implementazione array server completata
            }
        }
        clock.current = clock.next;  // Avanzamento del clock al valore del prossimo evento

        if (clock.current == clock.arrival) {
            process_arrival();
        } else {
            process_completion(*nextCompletion);
        }
    }
    end_servers();
    print_real_cost(&global_network_status);
    //print_statistics(&global_network_status, blocks, clock.current, &global_sorted_completions);
    calculate_statistics(&global_network_status, blocks, clock.current, &global_sorted_completions, response_times);
    print_line();
    for (int i = 0; i < 3; i++) {
        printf("slot #%d: System Total Response Time .......... = %1.6f\n", i, response_times[i]);
        statistics[repetition][i] = response_times[i];
    }
    clear_environment();
}

void infinite_horizon_simulation(int slot) {
    printf("\n\n==== Infinite Horizon Simulation for slot %d ====\n", slot);
    arrival_rate = lambdas[slot];
    global_network_status.time_slot = slot;
    init_config();
    init_network();
    update_network();
    int simulation_time = infinites[slot];
    double old;
    while (clock.arrival <= simulation_time) {
        print_percentage(clock.current, simulation_time, old);
        old = clock.current;
        compl *nextCompletion = &global_sorted_completions.sorted_list[0];
        server *nextCompletionServer = nextCompletion->server;
        clock.next = min(nextCompletion->value, clock.arrival);  // Ottengo il prossimo evento

        for (int i = 0; i < NUM_BLOCKS; i++) {
            if (blocks[i].jobInBlock > 0) {
                blocks[i].area.node += (clock.next - clock.current) * blocks[i].jobInBlock;
                blocks[i].area.queue += (clock.next - clock.current) * blocks[i].jobInQueue;
                //blocks[i].area.service += (clock.next - clock.current);  //TODO Da togliere ?? Utilizzazione non si calcola cosi, va calcolata per ogni server. La mettiamo a implementazione array server completata
            }
        }
        clock.current = clock.next;  // Avanzamento del clock al valore del prossimo evento
        if (clock.current == clock.arrival) {
            process_arrival();
        } else {
            process_completion(*nextCompletion);
        }
    }
    print_block_status(&global_sorted_completions, blocks, dropped, completed, bypassed);
    print_statistics(&global_network_status, blocks, clock.current, &global_sorted_completions);
}

/*
* Genera un tempo di Arrivo secondo la distribuzione di Poisson
*/
double getArrival(double current) {
    double arrival = current;
    SelectStream(254);
    arrival += Poisson(1 / arrival_rate);
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
Processa un arrivo dall'esterno verso il sistema
*/
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

// Inizializza tutti i blocchi del sistema
void init_network() {
    streamID = 0;
    clock.current = START;
    for (int i = 0; i < 3; i++) {
        slot_switched[i] = false;
        response_times[i] = 0;
    }
    init_blocks();

    if (str_compare(simulation_mode, "FINITE") == 0) {
        set_time_slot();
    }

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
void set_time_slot() {
    if (clock.current == START) {
        global_network_status.time_slot = 0;
        arrival_rate = LAMBDA_1;
        slot_switched[0] = true;
        update_network();
    }
    if (clock.current >= TIME_SLOT_1 && clock.current < TIME_SLOT_1 + TIME_SLOT_2 && !slot_switched[1]) {
        calculate_statistics(&global_network_status, blocks, clock.current, &global_sorted_completions, response_times);

        global_network_status.time_slot = 1;
        arrival_rate = LAMBDA_2;
        slot_switched[1] = true;
        update_network();
    }
    if (clock.current >= TIME_SLOT_1 + TIME_SLOT_2 && !slot_switched[2]) {
        calculate_statistics(&global_network_status, blocks, clock.current, &global_sorted_completions, response_times);

        global_network_status.time_slot = 2;
        arrival_rate = LAMBDA_3;
        slot_switched[2] = true;
        update_network();
    }
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