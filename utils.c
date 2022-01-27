#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "DES/rng.h"
#include "DES/rvgs.h"
#include "config.h"

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
        case SEASON_GATE:
            return GREEN_PASS;
        case TICKET_BUY:
            return TICKET_GATE;
        case TICKET_GATE:
            return GREEN_PASS;
        case GREEN_PASS:
            return EXIT;
            break;
    }
}

// Ritorna il minimo tra due valori
double min(double x, double y) {
    return (x < y) ? x : y;
}

// Scorre l'intera Linked List dei serventi
server *iterateOver(server *s) {
    server *current = s;
    while (current != NULL) {
        if (current->next != NULL) {
            current = current->next;
        } else
            break;
    }
    return current;
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
    printf("Genero il tempo di completamento: {(%d,%d),%f}\n", completion.server->block_type, completion.server->id, completion.value);

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
    printf("Eseguito il completamento: {(%d,%d),%f}\n", completion.server->block_type, completion.server->id, completion.value);

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
    printf("\nBusy Servers: %d | Dropped: %d | Completions: %d\n", compls->num_completions, dropped, completions);
    printf("TEMPERATURE | Enqueued Job: %d  Arrivals: %d  Completions: %d\n", blocks[0].jobInQueue, blocks[0].total_arrivals, blocks[0].total_completions);
    printf("TICKET_BUY  | Enqueued Job: %d  Arrivals: %d  Completions: %d\n", blocks[1].jobInQueue, blocks[1].total_arrivals, blocks[1].total_completions);
    printf("TICKET_GATE | Enqueued Job: %d  Arrivals: %d  Completions: %d\n", blocks[2].jobInQueue, blocks[2].total_arrivals, blocks[2].total_completions);
    printf("SEASON_GATE | Enqueued Job: %d  Arrivals: %d  Completions: %d\n", blocks[3].jobInQueue, blocks[3].total_arrivals, blocks[3].total_completions);
    printf("GREEN_PASS  | Enqueued Job: %d  Arrivals: %d  Completions: %d\n", blocks[4].jobInQueue, blocks[4].total_arrivals, blocks[4].total_completions);

    for (int i = 0; i < compls->num_completions; i++) {
        compl actual = compls->sorted_list[i];
        printf("(%d,%d)  %d  %f\n", actual.server->block_type, actual.server->id, actual.server->status, actual.value);
    }
    printf("\n");
}

double drand(double low, double high) {
    return ((double)rand() * (high - low)) / (double)100 + low;
}