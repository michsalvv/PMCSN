#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "DES/rng.h"
#include "DES/rvgs.h"
#include "config.h"

FILE *open_csv(char *filename);
FILE *open_csv_appendMode(char *filename);
void *append_on_csv(FILE *fpt, double ts, double p);
void *append_on_csv_v2(FILE *fpt, double ts, double p);

// Stampa a schermo una linea di separazione
void print_line() {
    printf("\n————————————————————————————————————————————————————————————————————————————————————————\n");
}

// TODO necessario perchè strcmp non viene letto da gdb
int str_compare(char *str1, char *str2) {
    while (*str1 && *str1 == *str2) {
        str1++;
        str2++;
    }
    return *str1 - *str2;
}

// Ritorna il nome del blocco passando il suo identificativo
char *stringFromEnum(enum block_types f) {
    char *strings[] = {"TEMPERATURE_CTRL", "TICKET_BUY", "SEASON_GATE", "TICKET_GATE", "GREEN_PASS"};
    return strings[f];
}

// Fornisce il codice del blocco di destinazione partendo dal blocco del controllo temperatura
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

// Ritorna il blocco destinazione di un job dopo il completamento
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

void print_real_cost(network_status *net) {
    double cm_costs[] = {CM_TEMPERATURE_CTRL_SERVER, CM_TICKET_BUY_SERVER, CM_SEASON_GATE_SERVER, CM_TICKET_GATE_SERVER, CM_GREEN_PASS_SERVER};
    double costs[5] = {0, 0, 0, 0, 0};
    double total = 0;
    float sec_in_month = 60 * 60 * 19 * 30;
    print_line();
    printf("Analisi Costi\n");
    for (int j = 0; j < NUM_BLOCKS; j++) {
        for (int i = 0; i < MAX_SERVERS; i++) {
            server s = net->server_list[j][i];
            costs[j] += s.time_online * (cm_costs[j] / sec_in_month);
        }
        total += costs[j];
        printf("....%s: %f\n", stringFromEnum(j), costs[j]);
    }
    printf("....\n....TOTALE: %f\n", total);
}

// Stampa i dettagli sui costi della configurazione
void print_cost_theor(network_configuration conf) {
    int seconds;
    int slots[] = {TIME_SLOT_1, TIME_SLOT_2, TIME_SLOT_3};
    float sec_in_month = 60 * 60 * 19 * 30;
    for (int slot = 0; slot < 3; slot++) {
        seconds = slots[slot];
        printf("\n-- Costo Fascia #%d --", slot);
        double temp_c = CM_TEMPERATURE_CTRL_SERVER / sec_in_month * conf.slot_config[slot][TEMPERATURE_CTRL] * seconds;
        printf("Costo Controllo Temperatura: %f\n", temp_c);

        double buy_c = CM_TICKET_BUY_SERVER / sec_in_month * conf.slot_config[slot][TICKET_BUY] * seconds;
        printf("Costo Acquisto Biglietti: %f\n", buy_c);

        double ticket_c = CM_TICKET_GATE_SERVER / sec_in_month * conf.slot_config[slot][TICKET_GATE] * seconds;
        printf("Costo Verifica Biglietti: %f\n", ticket_c);

        double season_c = CM_SEASON_GATE_SERVER / sec_in_month * conf.slot_config[slot][SEASON_GATE] * seconds;
        printf("Costo Verifica Abbonamenti: %f\n", season_c);

        double green_c = CM_GREEN_PASS_SERVER / sec_in_month * conf.slot_config[slot][GREEN_PASS] * seconds;
        printf("Costo Verifica Green Pass: %f\n", green_c);

        double total = temp_c + buy_c + ticket_c + season_c + green_c;
        printf("------ Costo TOTALE: %f\n", total);
    }
}

// Ritorna il minimo tra due valori
double min(double x, double y) {
    return (x < y) ? x : y;
}

/*
** Mette in pausa il programma aspettando l'input utente
*/
void waitInput() {
    if (DEBUG != 1)
        return;
    char c = getchar();
    while (getchar() != '\n')
        ;
}

// Pulisce lo schermo
void clearScreen() {
    printf("\033[H\033[2J");
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

// Function to delete an element
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

void print_block_status(sorted_completions *compls, struct block blocks[], int dropped, int completions, int bypassed) {
    printf("\n============================================================================================================\n");
    printf("Busy Servers: %d | Dropped: %d | Completions: %d | Bypassed: %d\n", compls->num_completions, dropped, completions, bypassed);
    printf("TEMPERATURE | Block Job: %d  Enqueued Job: %d  Arrivals: %d  Completions: %d\n", blocks[0].jobInBlock, blocks[0].jobInQueue, blocks[0].total_arrivals, blocks[0].total_completions);
    printf("TICKET_BUY  | Block Job: %d  Enqueued Job: %d  Arrivals: %d  Completions: %d\n", blocks[1].jobInBlock, blocks[1].jobInQueue, blocks[1].total_arrivals, blocks[1].total_completions);
    printf("SEASON_GATE | Block Job: %d  Enqueued Job: %d  Arrivals: %d  Completions: %d\n", blocks[2].jobInBlock, blocks[2].jobInQueue, blocks[2].total_arrivals, blocks[2].total_completions);
    printf("TICKET_GATE | Block Job: %d  Enqueued Job: %d  Arrivals: %d  Completions: %d\n", blocks[3].jobInBlock, blocks[3].jobInQueue, blocks[3].total_arrivals, blocks[3].total_completions);
    printf("GREEN_PASS  | Block Job: %d  Enqueued Job: %d  Arrivals: %d  Completions: %d\n", blocks[4].jobInBlock, blocks[4].jobInQueue, blocks[4].total_arrivals, blocks[4].total_completions);
    printf("============================================================================================================\n\n");

    printf("\n");
}

// Stampa lo stato attuale dei completamenti da processare
void print_completion_status(sorted_completions *compls) {
    for (int i = 0; i < compls->num_completions; i++) {
        compl actual = compls->sorted_list[i];
        printf("(%d,%d)  %d  %f\n", actual.server->block->type, actual.server->id, actual.server->status, actual.value);
    }
}

// Verifica che il numero di job in ingresso ai vari blocchi è coerente con le probabilità di routing.
void debug_routing() {
    int numExit = 0;
    int numTicketBuy = 0;
    int numTicketGate = 0;
    int numSeasonGate = 0;

    for (int i = 0; i < 1000; i++) {
        int res = routing_from_temperature();
        if (res == EXIT) {
            numExit++;
        }
        if (res == TICKET_BUY) {
            numTicketBuy++;
        }
        if (res == TICKET_GATE) {
            numTicketGate++;
        }
        if (res == SEASON_GATE) {
            numSeasonGate++;
        }
    }
    printf("Exit: %d\nTickBuy: %d\nSeasGate: %d\nTickGate: %d\n", numExit, numTicketBuy, numTicketGate, numSeasonGate);
    exit(0);
}

void calculate_statistics_clock(network_status *network, struct block blocks[], double currentClock) {
    char filename[21];
    snprintf(filename, 21, "continuos_finite.csv");
    FILE *csv;
    csv = open_csv_appendMode(filename);

    double system_total_wait = 0;
    for (int i = 0; i < NUM_BLOCKS; i++) {
        int m = network->num_online_servers[i];
        int arr = blocks[i].total_arrivals;
        int r_arr = arr - blocks[i].total_bypassed;
        int jq = blocks[i].jobInQueue;
        int inter = currentClock / blocks[i].total_arrivals;

        double a_rate = blocks[i].total_arrivals / currentClock;
        double ra_rate = r_arr / currentClock;
        double s_rate = r_arr / blocks[i].area.service;

        double wait = blocks[i].area.node / r_arr;
        double delay = blocks[i].area.queue / r_arr;
        double service = blocks[i].area.service / r_arr;
        double utilization = ra_rate / (m * s_rate);

        system_total_wait += wait;
    }
    append_on_csv_v2(csv, system_total_wait, currentClock);
    fclose(csv);
}

// Calcola le statistiche specificate
void calculate_statistics_fin(network_status *network, struct block blocks[], double currentClock, double rt_arr[]) {
    char type[20];
    double system_total_wait = 0;
    for (int i = 0; i < NUM_BLOCKS; i++) {
        strcpy(type, stringFromEnum(blocks[i].type));

        int m = network->num_online_servers[i];
        int arr = blocks[i].total_arrivals;
        int r_arr = arr - blocks[i].total_bypassed;
        int jq = blocks[i].jobInQueue;
        int inter = currentClock / blocks[i].total_arrivals;

        double a_rate = blocks[i].total_arrivals / currentClock;
        double ra_rate = r_arr / currentClock;
        double s_rate = r_arr / blocks[i].area.service;

        double wait = blocks[i].area.node / r_arr;
        double delay = blocks[i].area.queue / r_arr;
        double service = blocks[i].area.service / r_arr;
        double utilization = ra_rate / (m * s_rate);

        system_total_wait += wait;
    }
    rt_arr[network->time_slot] = system_total_wait;
}

// Calcola le statistiche specificate
void calculate_statistics_inf(network_status *network, struct block blocks[], double currentClock, double rt_arr[], int pos) {
    char type[20];
    double system_total_wait = 0;
    for (int i = 0; i < NUM_BLOCKS; i++) {
        strcpy(type, stringFromEnum(blocks[i].type));

        int m = network->num_online_servers[i];
        int arr = blocks[i].total_arrivals;
        int r_arr = arr - blocks[i].total_bypassed;
        int jq = blocks[i].jobInQueue;
        int inter = currentClock / blocks[i].total_arrivals;

        double wait = blocks[i].area.node / arr;
        double delay = blocks[i].area.queue / r_arr;
        double service = blocks[i].area.service / r_arr;

        system_total_wait += wait;
    }
    rt_arr[pos] = system_total_wait;
}

// Stampa a schermo le statistiche calcolate
void print_statistics(network_status *network, struct block blocks[], double currentClock, sorted_completions *compls, int rep) {
    char type[20];
    double system_total_wait = 0;
    for (int i = 0; i < NUM_BLOCKS; i++) {
        strcpy(type, stringFromEnum(blocks[i].type));

        int m = network->num_online_servers[i];
        int arr = blocks[i].total_arrivals;
        int r_arr = arr - blocks[i].total_bypassed;
        int jq = blocks[i].jobInQueue;
        double inter = currentClock / blocks[i].total_arrivals;

        double wait = blocks[i].area.node / r_arr;
        double delay = blocks[i].area.queue / r_arr;
        double service = blocks[i].area.service / r_arr;

        system_total_wait += wait;

        printf("\n\n======== Result for block %s ========\n", type);
        printf("Number of Servers ................... = %d\n", m);
        printf("Arrivals ............................ = %d\n", arr);
        printf("Job in Queue at the end ............. = %d\n", jq);
        printf("Average interarrivals................ = %6.6f\n", inter);

        printf("Average wait ........................ = %6.6f\n", wait);
        if (i == GREEN_PASS) {
            printf("Average wait (2)..................... = %6.6f\n", blocks[i].area.node / blocks[i].total_arrivals);
            printf("Number bypassed ..................... = %d\n", blocks[i].total_bypassed);
        }
        printf("Average delay ....................... = %6.6f\n", delay);
        printf("Average service time ................ = %6.6f\n", service);

        printf("Average # in the queue .............. = %6.6f\n", blocks[i].area.queue / currentClock);
        printf("Average # in the node ............... = %6.6f\n", blocks[i].area.node / currentClock);

        printf("\n    server     utilization     avg service\n");
        for (int j = 0; j < MAX_SERVERS; j++) {
            server s = network->server_list[i][j];
            if (s.used == 1) {
                printf("%8d %15.5f %15.2f\n", s.id, (s.sum.service / currentClock), (s.sum.service / s.sum.served));
            }
        }
    }
    printf("\nSlot #%d: System Total Response Time .......... = %1.6f\n", network->time_slot, system_total_wait);
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

// Stampa a schermo lo stato di tutti i serventi della rete
void print_network_status(network_status *network) {
    printf("\n");
    for (int j = 0; j < NUM_BLOCKS; j++) {
        for (int i = 0; i < MAX_SERVERS; i++) {
            server s = network->server_list[j][i];
            if (s.used == NOTUSED) {
                break;
            }
            printf("(%d,%d) | status: {%d,%d} t_onl: %f\n", s.block->type, s.id, s.status, s.online, s.time_online);
        }
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

// Apre un file csv e ritorna il puntatore a quel file
FILE *open_csv(char *filename) {
    FILE *fpt;
    fpt = fopen(filename, "w");
    return fpt;
}

FILE *open_csv_appendMode(char *filename) {
    FILE *fpt;
    fpt = fopen(filename, "a");
    return fpt;
}

// Inserisce una nuova linea nel file csv specificato
void *append_on_csv(FILE *fpt, double ts, double p) {
    fprintf(fpt, "%2.6f\n", ts);
    return fpt;
}

void *append_on_csv_v2(FILE *fpt, double ts, double p) {
    fprintf(fpt, "%2.6f; %2.6f\n", ts, p);
    return fpt;
}

// Stampa la configurazione di avvio
void print_configuration(network_configuration *config) {
    for (int slot = 0; slot < 3; slot++) {
        printf("FASCIA #%d\n", slot);
        for (int block = 0; block < NUM_BLOCKS; block++) {
            printf("...%s: %d\n", stringFromEnum(block), config->slot_config[slot][block]);
        }
        printf("\n");
    }
}