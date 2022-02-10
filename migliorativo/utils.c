#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "./DES/rng.h"
#include "./DES/rvgs.h"
#include "./config.h"

FILE *open_csv(char *filename);
FILE *open_csv_appendMode(char *filename);
void *append_on_csv(FILE *fpt, double ts, double p);
void *append_on_csv_v2(FILE *fpt, double ts, double p);

// Ritorna il minimo tra due valori
double min(double x, double y) {
    return (x < y) ? x : y;
}

// Genera una configurazione di avvio per la simulazione
network_configuration get_config(int *values_1, int *values_2, int *values_3) {
    network_configuration *config = malloc(sizeof(network_configuration));
    for (int i = 0; i < NUM_BLOCKS; i++) {
        config->slot_config[0][i] = values_1[i];
        config->slot_config[1][i] = values_2[i];
        config->slot_config[2][i] = values_3[i];
    }
    return *config;
}

// Restituisce il codice del blocco di destinazione partendo dal blocco del controllo temperatura
int routing_from_temperature() {
    double random = Uniform(0, 100);
    if (random < P_EXIT_TEMP) {
        return EXIT;
    } else if (random < P_EXIT_TEMP + P_TICKET_BUY) {
        return TICKET_BUY;
    } else if (random < P_EXIT_TEMP + P_TICKET_BUY + P_SEASON_GATE) {
        return SEASON_GATE;
    } else {
        return TICKET_GATE;
    }
}

// Ricerca binaria di un elemento su una lista ordinata
int binarySearch(sorted_completions *compls, int low, int high, compl completion) {
    if (high < low) {
        return -1;
    }

    int mid = (low + high) / 2;
    if (completion.value == compls->sorted_list[mid].value) {
        return mid;
    }
    if (completion.value == compls->sorted_list[mid].value) {
        return binarySearch(compls, (mid + 1), high, completion);
    }
    return binarySearch(compls, low, (mid - 1), completion);
}

// Elimina un elemento dalla lista ordinata
int deleteElement(sorted_completions *compls, compl completion) {
    int n = compls->num_completions;

    int pos = binarySearch(compls, 0, n - 1, completion);

    if (pos == -1) {
        printf("Element not found");
        return n;
    }

    // Deleting element
    int i;
    for (i = pos; i < n; i++) {
        compls->sorted_list[i] = compls->sorted_list[i + 1];
    }
    compls->sorted_list[n - 1].value = INFINITY;
    compls->num_completions--;

    return n - 1;
}

// Ritorna il blocco destinazione di un job dopo il suo completamento
int getDestination(enum block_types from) {
    switch (from) {
        case TEMPERATURE_CTRL:
            return routing_from_temperature();
        case TICKET_BUY:
            return TICKET_GATE;
        case SEASON_GATE:
            return GREEN_PASS;
        case TICKET_GATE:
            return GREEN_PASS;
        case GREEN_PASS:
            return EXIT;
            break;
    }
}

// Inserisce un elemento nella lista ordinata
int insertSorted(sorted_completions *compls, compl completion) {
    int i;
    int n = compls->num_completions;

    for (i = n - 1; (i >= 0 && (compls->sorted_list[i].value > completion.value)); i--) {
        compls->sorted_list[i + 1] = compls->sorted_list[i];
    }
    compls->sorted_list[i + 1] = completion;
    compls->num_completions++;

    return (n + 1);
}

// Rirtona la stringa corrispondente al blocco ricevuto in input
char *stringFromEnum(enum block_types f) {
    char *strings[] = {"TEMPERATURE_CTRL", "TICKET_BUY", "SEASON_GATE", "TICKET_GATE", "GREEN_PASS"};
    return strings[f];
}

// Rirtona la stringa corrispondente allo stato ricevuto in input
char *stringFromStatus(int status) {
    if (status)
        return "BUSY";
    return "IDLE";
}

// Ritorna la stringa corrispondente allo stato ricevuto in input
char *stringFromOnline(int online) {
    if (online)
        return "ONLINE";
    return "OFFLINE";
}

// Stampa a schermo le informazioni relative ad ogni singolo server del sistema
void printServerInfo(network_status network, int blockType) {
    int n = network.num_online_servers[blockType];
    printf("\n%s\n", stringFromEnum(blockType));
    for (int i = 0; i < n; i++) {
        server s = network.server_list[blockType][i];
        printf("Server %d | jobInQueue %d | status %d | jobInTotal %d\n", s.id, s.jobInQueue, s.status, s.jobInTotal);
    }
}

void print_single_server_info(server s) {
    printf("Block %s | Server %d | jobInQueue %d | jobInTotal %d | %s\n", stringFromEnum(s.block->type), s.id, s.jobInQueue, s.jobInTotal, stringFromStatus(s.status));
}

void print_network_status(network_status *network) {
    printf("\n");
    for (int j = 0; j < NUM_BLOCKS; j++) {
        for (int i = 0; i < MAX_SERVERS; i++) {
            server s = network->server_list[j][i];
            if (s.used == NOTUSED) {
                break;
            }
            printf("(%d,%d) | %s | %s | jobInQueue :%d | jobInTotal: %d | arrivals: %d | completions: %d | temp_online: %f | need_resched: %d\n", s.block->type, s.id, stringFromStatus(s.status), stringFromOnline(s.online), s.jobInQueue, s.jobInTotal, s.arrivals, s.completions, s.time_online, s.need_resched);
        }
    }
}

// Stampa a schermo la configurazione in utilizzo
void print_configuration(network_configuration *config) {
    for (int slot = 0; slot < 3; slot++) {
        printf("\nFASCIA #%d\n", slot);
        for (int block = 0; block < NUM_BLOCKS; block++) {
            printf("...%s: %d\n", stringFromEnum(block), config->slot_config[slot][block]);
        }
    }
}

int str_compare(char *str1, char *str2) {
    while (*str1 && *str1 == *str2) {
        str1++;
        str2++;
    }
    return *str1 - *str2;
}

// Calcola e stampa a schermo le statistiche di ogni singolo server
void print_servers_statistics(network_status *network, double end_slot, double currentClock) {
    double system_total_wait = 0;
    for (int i = 0; i < NUM_BLOCKS; i++) {
        double p = 0;
        int n = 0;
        for (int j = 0; j < MAX_SERVERS; j++) {
            server s = network->server_list[i][j];
            if (!s.used)
                break;

            int arr = s.arrivals;
            int jq = s.jobInQueue;

            double wait = s.area.node / arr;
            double delay = s.area.queue / arr;
            double service = s.area.service / arr;
            double u = min(s.area.service / currentClock, 1);

            printf("\n======== %s server %d ========\n", stringFromEnum(i), j);
            printf("Arrivals ............................ = %d\n", arr);
            printf("Completions.......................... = %d\n", s.completions);
            printf("Job in Queue at the end ............. = %d\n", jq);
            printf("Server status at the end ............ = %s %s\n", stringFromStatus(s.status), stringFromOnline(s.online));
            printf("Turned OFF at time .................. = %6.6f\n", s.time_online);

            printf("Average wait ........................ = %6.6f\n", wait);
            printf("Average delay ....................... = %6.6f\n", delay);
            printf("Average service time ................ = %6.6f\n", service);

            printf("Average # in the queue .............. = %6.6f\n", s.area.queue / currentClock);
            printf("Average # in the node ............... = %6.6f\n", s.area.node / currentClock);
            printf("Utilization ......................... = %6.6f\n", u);

            p += u;
            n++;
        }
        printf("\nBlock %s: Mean Utilization .................... = %1.6f\n", stringFromEnum(i), p / n);
    }
}

// Calcola le statistiche ogni 5 minuti per l'analisi del continuo
void calculate_statistics_clock(network_status *network, struct block blocks[], double currentClock, FILE *csv) {
    double external_arrival_rate = 1 / (currentClock / blocks[TEMPERATURE_CTRL].total_arrivals);
    double visit_rt = 0;

    for (int i = 0; i < NUM_BLOCKS - 1; i++) {
        double block_mean_wait = 0;
        double wait = 0;

        double visit_sum = 0;
        int arrival_sum = 0;
        int compl = 0;

        for (int j = 0; j < MAX_SERVERS; j++) {
            server *s = &network->server_list[i][j];
            if (!s->used) {
                break;
            } else {
                if (s->arrivals > 0) {
                    double lambda = (double)s->arrivals / currentClock;
                    double mu = s->arrivals / s->area.service;
                    double throughput = min(mu, lambda);

                    wait = s->area.node / s->arrivals;

                    double visit = throughput / external_arrival_rate;
                    visit_sum += visit;
                    arrival_sum += s->arrivals;
                    compl += s->completions;
                    visit_rt += visit * wait;
                }
            }
        }
    }

    double lambda_green = blocks[GREEN_PASS].total_arrivals / currentClock;
    double visit_green = lambda_green / external_arrival_rate;
    double wait = blocks[GREEN_PASS].area.node / blocks[GREEN_PASS].total_arrivals;
    visit_rt += visit_green * wait;

    append_on_csv_v2(csv, visit_rt, currentClock);
}

FILE *open_csv_appendMode(char *filename) {
    FILE *fpt;
    fpt = fopen(filename, "aw");
    return fpt;
}

void *append_on_csv_v2(FILE *fpt, double ts, double p) {
    fprintf(fpt, "%2.6f; %2.6f\n", ts, p);
    return fpt;
}

// Calcola i costi del blocco in base al tempo passato online da ogni singolo server
double calculate_cost(network_status *net) {
    double cm_costs[] = {CM_TEMPERATURE_CTRL_SERVER, CM_TICKET_BUY_SERVER, CM_SEASON_GATE_SERVER, CM_TICKET_GATE_SERVER, CM_GREEN_PASS_SERVER};
    double costs[5] = {0, 0, 0, 0, 0};
    double total = 0;
    float sec_in_month = 60 * 60 * 19 * 30;
    for (int j = 0; j < NUM_BLOCKS; j++) {
        for (int i = 0; i < MAX_SERVERS; i++) {
            server s = net->server_list[j][i];
            costs[j] += s.time_online * (cm_costs[j] / sec_in_month);
        }
        total += costs[j];
    }
    return total;
}

// Calcola le stastitiche specificate ad orizzonte finito
void calculate_statistics_fin(network_status *network, double currentClock, double rt_arr[], double p_arr[NUM_REPETITIONS][3][NUM_BLOCKS], int rep) {
    double temperature_arrivals = network->server_list[0][0].block->total_arrivals;
    double external_arrival_rate = 1 / (currentClock / temperature_arrivals);
    double visit_rt = 0;

    for (int i = 0; i < NUM_BLOCKS - 1; i++) {
        double block_mean_wait = 0;
        double wait = 0;
        int servers = 0;
        double p = 0;
        double visit_sum = 0;
        int arrival_sum = 0;
        int compl = 0;

        for (int j = 0; j < MAX_SERVERS; j++) {
            server *s = &network->server_list[i][j];
            if (!s->used) {
                break;
            } else {
                if (s->arrivals > 0) {
                    double lambda = (double)s->arrivals / currentClock;
                    double mu = s->arrivals / s->area.service;
                    double throughput = min(mu, lambda);

                    wait = s->area.node / s->arrivals;

                    double visit = throughput / external_arrival_rate;
                    visit_sum += visit;
                    arrival_sum += s->arrivals;
                    compl += s->completions;
                    visit_rt += visit * wait;
                    p += (s->area.service / currentClock);
                    servers++;
                }
            }
            p_arr[rep][network->time_slot][i] = p / servers;
        }
    }

    struct block *green_pass = network->server_list[4][0].block;
    double lambda_green = green_pass->total_arrivals / currentClock;
    double visit_green = lambda_green / external_arrival_rate;
    double wait = green_pass->area.node / green_pass->total_arrivals;
    visit_rt += visit_green * wait;
    rt_arr[network->time_slot] = visit_rt;

    double p_green = 0;
    int servers_green = 0;
    for (int q = 0; q < MAX_SERVERS; q++) {
        server *s = &network->server_list[GREEN_PASS][q];
        if (!s->used) {
            break;
        } else {
            p_green += (s->area.service / currentClock);
        }
        servers_green++;
    }

    p_arr[rep][network->time_slot][GREEN_PASS] = p_green / servers_green;
}

// Calcola le stastitiche specificate ad orizzonte infinito
void calculate_statistics_inf(network_status *network, struct block blocks[], double currentClock, double rt_arr[], int pos) {
    double external_arrival_rate = 1 / (currentClock / blocks[TEMPERATURE_CTRL].total_arrivals);
    double visit_rt = 0;
    for (int i = 0; i < NUM_BLOCKS - 1; i++) {
        double block_mean_wait = 0;
        double wait = 0;
        int servers = 0;
        double visit_sum = 0;
        int arrival_sum = 0;
        int compl = 0;
        for (int j = 0; j < MAX_SERVERS; j++) {
            server *s = &network->server_list[i][j];
            if (!s->used) {
                break;
            } else {
                if (s->arrivals > 0) {
                    double lambda = (double)s->arrivals / currentClock;
                    double mu = s->arrivals / s->area.service;
                    double throughput = min(mu, lambda);

                    wait = s->area.node / s->arrivals;

                    double visit = throughput / external_arrival_rate;
                    visit_sum += visit;
                    arrival_sum += s->arrivals;
                    compl += s->completions;
                    visit_rt += visit * wait;
                }
            }
        }
    }

    double lambda_green = blocks[GREEN_PASS].total_arrivals / currentClock;
    double visit_green = lambda_green / external_arrival_rate;
    double wait = blocks[GREEN_PASS].area.node / blocks[GREEN_PASS].total_arrivals;
    visit_rt += visit_green * wait;

    rt_arr[pos] = visit_rt;
}

// Stampa tutte le tuilizzazioni ad orizzonte finito su un file csv
void print_p_on_csv(network_status *network, double currentClock, int slot) {
    FILE *csv;
    char filename[100];

    for (int i = 0; i < NUM_BLOCKS; i++) {
        double p = 0;
        snprintf(filename, 100, "results/finite/u_%d_finite_slot%d.csv", i, slot);
        csv = open_csv(filename);
        for (int j = 0; j < MAX_SERVERS; j++) {
            server *s = &network->server_list[i][j];
            if (!s->used) {
                break;
            } else {
                if (s->arrivals > 0) {
                    p = (s->area.service / currentClock);
                    append_on_csv(csv, p, 0);
                }
            }
        }
        fclose(csv);
    }
}

// Stampa una barra di avanzamento relativa alla singola run di simulazione
void print_percentage(double part, double total, double oldPart) {
    double percentage = part / total * 100;
    double oldPercentage = oldPart / total * 100;

    if ((int)oldPercentage == (int)percentage) {
        return;
    }
    printf("\rSimulation Progress: |");
    for (int i = 0; i <= percentage / 2; i++) {
        printf("█");
        fflush(stdout);
    }
    for (int j = percentage / 2; j < 50 - 1; j++) {
        printf(" ");
    }
    printf("|");
    printf(" %02.0f%%", percentage + 1);
}

// Ritorna un puntatore al file appena aperta
FILE *open_csv(char *filename) {
    FILE *fpt;
    fpt = fopen(filename, "w+");
    return fpt;
}

// Inserisce in append una nuova linea al csv
void *append_on_csv(FILE *fpt, double ts, double p) {
    fprintf(fpt, "%2.6f\n", ts);
    return fpt;
}
