#include <stdio.h>

#include "config.h"
#include "structures.h"

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