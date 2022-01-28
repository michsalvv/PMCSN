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

char *stringFromEnum(enum block_types f) {
    char *strings[] = {"TEMPERATURE_CTRL", "TICKET_BUY", "SEASON_GATE", "TICKET_GATE", "GREEN_PASS"};
    return strings[f];
}

// Fornisce il blocco di destinazione partendo dal blocco del controllo temperatura
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
/*
double calculate_cost(int seconds) {
    // Numero di secondi in un mese sulle 19 ore lavorative
    float sec_in_month = 60 * 60 * 19 * 30;
    double temp_c = CM_TEMPERATURE_CTRL_SERVER / sec_in_month * TEMPERATURE_CTRL_SERVERS * seconds;
    printf("c_temp: %f\n", temp_c);
    double buy_c = CM_TICKET_BUY_SERVER / sec_in_month * TICKET_BUY_SERVERS * seconds;
    printf("c_buy: %f\n", buy_c);
    double ticket_c = CM_TICKET_GATE_SERVER / sec_in_month * TICKET_GATE_SERVERS * seconds;
    printf("c_tick: %f\n", ticket_c);
    double season_c = CM_SEASON_GATE_SERVER / sec_in_month * SEASON_GATE_SERVERS * seconds;
    printf("c_seas: %f\n", season_c);
    double green_c = CM_GREEN_PASS_SERVER / sec_in_month * GREEN_PASS_SERVERS * seconds;
    printf("c_green: %f\n", green_c);

    return temp_c + buy_c + ticket_c + season_c + green_c;
}
*/

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
    printf("Genero il tempo di completamento: {(%d,%d),%f}\n", completion.server->block->type, completion.server->id, completion.value);

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
    // Find position of element to be deleted
    printf("Eseguito il completamento: {(%d,%d),%f}\n", completion.server->block->type, completion.server->id, completion.value);

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

void print_completions_status(sorted_completions *compls, struct block blocks[], int dropped, int completions) {
    printf("\n==============================================================================\n");
    printf("Busy Servers: %d | Dropped: %d | Completions: %d\n", compls->num_completions, dropped, completions);
    //printf("Total Configuration Cost: %f\n", calculate_cost(TIME_SLOT_1));
    printf("TEMPERATURE | Enqueued Job: %d  Arrivals: %d  Completions: %d\n", blocks[0].jobInQueue, blocks[0].total_arrivals, blocks[0].total_completions);
    printf("TICKET_BUY  | Enqueued Job: %d  Arrivals: %d  Completions: %d\n", blocks[1].jobInQueue, blocks[1].total_arrivals, blocks[1].total_completions);
    printf("SEASON_GATE | Enqueued Job: %d  Arrivals: %d  Completions: %d\n", blocks[2].jobInQueue, blocks[2].total_arrivals, blocks[2].total_completions);
    printf("TICKET_GATE | Enqueued Job: %d  Arrivals: %d  Completions: %d\n", blocks[3].jobInQueue, blocks[3].total_arrivals, blocks[3].total_completions);
    printf("GREEN_PASS  | Enqueued Job: %d  Arrivals: %d  Completions: %d\n", blocks[4].jobInQueue, blocks[4].total_arrivals, blocks[4].total_completions);
    printf("==============================================================================\n");

    for (int i = 0; i < compls->num_completions; i++) {
        compl actual = compls->sorted_list[i];
        printf("(%d,%d)  %d  %f\n", actual.server->block->type, actual.server->id, actual.server->status, actual.value);
    }
    printf("\n");
}

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
    printf("Exit: %d\nTickBuy: %d\nTickGate: %d\nSeasGate: %d\n", numExit, numTicketBuy, numTicketGate, numSeasonGate);
    exit(0);
}

void print_statistics(network_status *network, struct block blocks[], double currentClock /*.sorted_completions *compls*/) {
    char type[20];
    for (int i = 0; i < NUM_BLOCKS; i++) {
        strcpy(type, stringFromEnum(blocks[i].type));

        printf("\n\n======== Result for block %s ========\n", type);
        printf("Number of Servers ................... = %6.2d\n", network->num_online_servers[i]);
        printf("Arrivals ............................ = %6.2d\n", blocks[i].total_arrivals);
        printf("Job in Queue at the end ............. = %6.2d\n", blocks[i].jobInQueue);
        printf("Average interarrivlas................ = %6.2f\n", currentClock / blocks[i].total_arrivals);

        printf("Average wait ........................ = %6.2f\n", blocks[i].area.node / blocks[i].total_arrivals);
        printf("Average delay ....................... = %6.2f\n", blocks[i].area.queue / blocks[i].total_arrivals);
        printf("Average service time ................ = %6.2f\n", blocks[i].area.service / blocks[i].total_arrivals);

        printf("Average # in the queue .............. = %6.2f\n", blocks[i].area.queue / currentClock);
        printf("Average # in the node ............... = %6.2f\n", blocks[i].area.node / currentClock);

        // for (int j = 0; j < blocks[i].num_server; j++) {
        //     printf("Utilization of server %d = %f", j, (compls->sorted_list[j].server->sum.service/currentClock);
        // }

        //printf("Utilization ......................... = %6.2f\n", blocks[i].area.service / (currentClock * blocks[i].num_server));
    }
}

// Genera una configurazione di lancio
network_configuration get_config(int *values_1, int *values_2, int *values_3) {
    network_configuration *config = malloc(sizeof(network_configuration));
    for (int i = 0; i < NUM_BLOCKS; i++) {
        config->slot_config[0][i] = values_1[i];
        config->slot_config[1][i] = values_2[i];
        config->slot_config[2][i] = values_3[i];
    }
    return *config;
}