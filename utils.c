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
