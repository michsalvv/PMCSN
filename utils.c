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
void printServerList(server *s) {
    server *current = s;

    while (current != NULL) {
        printf("Server #%d \tStatus: %d\tCompletion: %f\n", current->id, current->status, current->completion);
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
