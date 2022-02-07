#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../DES/rng.h"
#include "../DES/rvgs.h"
#include "config_2.h"

FILE *open_csv(char *filename);
FILE *open_csv_appendMode(char *filename);
void *append_on_csv(FILE *fpt, double ts, double p);
void *append_on_csv_v2(FILE *fpt, double ts, double p);

double min(double x, double y) {
    return (x < y) ? x : y;
}

network_configuration get_config(int *values_1, int *values_2, int *values_3) {
    network_configuration *config = malloc(sizeof(network_configuration));
    for (int i = 0; i < NUM_BLOCKS; i++) {
        config->slot_config[0][i] = values_1[i];
        config->slot_config[1][i] = values_2[i];
        config->slot_config[2][i] = values_3[i];
    }
    return *config;
}

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
char *stringFromEnum(enum block_types f) {
    char *strings[] = {"TEMPERATURE_CTRL", "TICKET_BUY", "SEASON_GATE", "TICKET_GATE", "GREEN_PASS"};
    return strings[f];
}

char *stringFromStatus(int status) {
    if (status)
        return "BUSY";
    return "IDLE";
}

char *stringFromOnline(int online) {
    if (online)
        return "ONLINE";
    return "OFFLINE";
}

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
            double u = min(s.sum.service / end_slot, 1);

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

void calculate_statistics_clock(network_status *network, struct block blocks[], double currentClock, FILE *csv) {
    // char filename[21];
    // snprintf(filename, 21, "continuos_finite.csv");
    // FILE *csv;
    // csv = open_csv_appendMode(filename);

    double system_total_wait = 0;
    for (int i = 0; i < NUM_BLOCKS; i++) {
        double block_mean_wait = 0;
        double wait = 0;
        int servers = 0;
        for (int j = 0; j < MAX_SERVERS; j++) {
            server *s = &network->server_list[i][j];
            if (!s->used) {
                break;
            } else {
                if (s->arrivals > 0)
                    wait += s->area.node / s->arrivals;
                servers++;
            }
        }
        block_mean_wait = wait / servers;
        system_total_wait += block_mean_wait;
    }
    append_on_csv_v2(csv, system_total_wait, currentClock);
    // fclose(csv);
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

void calculate_statistics_fin(network_status *network, double currentClock, double rt_arr[], double p_arr[NUM_REPETITIONS][3][NUM_BLOCKS], int rep) {
    double system_total_wait = 0;
    for (int i = 0; i < NUM_BLOCKS; i++) {
        double block_mean_wait = 0;
        double wait = 0;
        int servers = 0;
        double p = 0;
        for (int j = 0; j < MAX_SERVERS; j++) {
            server *s = &network->server_list[i][j];
            if (!s->used) {
                break;
            } else {
                if (s->arrivals > 0)
                    wait += s->area.node / s->arrivals;

                p += (s->area.service / currentClock);
                servers++;
            }
        }
        p_arr[rep][network->time_slot][i] = p / servers;
        block_mean_wait = wait / servers;
        system_total_wait += block_mean_wait;
    }

    rt_arr[network->time_slot] = system_total_wait;
}

void calculate_statistics_inf(network_status *network, struct block blocks[], double currentClock, double rt_arr[], int pos) {
    double system_total_wait = 0;

    for (int i = 0; i < NUM_BLOCKS; i++) {
        double block_mean_wait = 0;
        double wait = 0;
        int servers = 0;
        for (int j = 0; j < MAX_SERVERS; j++) {
            server *s = &network->server_list[i][j];
            if (!s->used) {
                break;
            } else {
                if (s->arrivals > 0)
                    wait += s->area.node / s->arrivals;

                servers++;
            }
        }
        block_mean_wait = wait / servers;
        system_total_wait += block_mean_wait;
    }
    rt_arr[pos] = system_total_wait;
}

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

FILE *open_csv(char *filename) {
    FILE *fpt;
    fpt = fopen(filename, "w+");
    return fpt;
}

void *append_on_csv(FILE *fpt, double ts, double p) {
    fprintf(fpt, "%2.6f\n", ts);
    return fpt;
}
