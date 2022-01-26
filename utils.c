#include <stdio.h>

#include "config.h"

enum node_type getDestination(enum node_type from) {
    switch (from) {
        case TEMPERATURE_CTRL:
            // TODO aggiungere probabilità
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

// Stampa la lista dei server con il relativo stato
void printServerList(struct node block) {
    server *current = block.firstServer;
    printf("\n-- Blocco #%d #jobInQueue: %d--\n", block.type, block.jobInQueue);
    while (current != NULL) {
        printf("Server #%d \tStatus: %d\tCompletion: %f\t\t Type: %d\tStream: %d\n", current->id, current->status, current->completion, current->nodeType, current->stream);
        if (current->next == NULL) break;
        current = current->next;
    }
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

void clearScreen() {
    printf("\033[H\033[2J");
}

typedef struct {
    double value;
    server *server;
} completion;

typedef struct {
    completion sorted[TOTAL_SERVERS];
    int num_completion;
} sorted_completions;

// Inserts a key in arr[] of given capacity.  n is current
// size of arr[]. This function returns n+1 if insertion
// is successful, else n.
int insertSorted(sorted_completions *compls, completion key) {
    printf("inserting sorted: %f,%f\n", key.value, key.server);

    int i;
    int n = compls->num_completion;

    for (i = n - 1; (i >= 0 && (compls->sorted[i].value > key.value)); i--) {
        compls->sorted[i + 1] = compls->sorted[i];
    }
    compls->sorted[i + 1] = key;
    compls->num_completion++;

    return (n + 1);
}