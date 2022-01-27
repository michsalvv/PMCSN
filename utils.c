#include <math.h>
#include <stdbool.h>
#include <stdio.h>

#include "config.h"

// Ritorna il blocco destinazione di un job dopo il completamento
enum node_type getDestination(enum node_type from) {
    switch (from) {
        case TEMPERATURE_CTRL:
            return TICKET_BUY;
            break;

        default:
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
    printf("inserting in sorted list value: {(%d,%d),%f}\n", completion.server->nodeType, completion.server->id, completion.value);

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
    printf("deleting server_list: %f\n", completion.value);

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

void print_completions_status(sorted_completions *compls, int num, struct node blocks[]) {
    printf("\nBusy Servers: %d\n", compls->num_completions);
    printf("Enqueued Job TEMPERATURE: %d\n", blocks[TEMPERATURE_CTRL].jobInQueue);
    printf("Enqueued Job TICKET_BUY : %d\n", blocks[TICKET_BUY].jobInQueue);

    for (int i = 0; i < num; i++) {
        compl actual = compls->sorted_list[i];
        printf("(%d,%d)  %d  %f\n", actual.server->nodeType, actual.server->id, actual.server->status, actual.value);
    }
    printf("\n");
}

// Genera un numero casuale e ritorna true o false in base alla probabilita di perdita passata in input
bool routing_to(int prob) {
    int random = rand() % 100;
    if (random < prob) {
        return true;
    }
    return false;
}