#include <stdio.h>

#include "config.h"

typedef struct {
    server sorted[TOTAL_SERVERS];
    int num_completion;
    server *block_heads[5];
} sorted_completions;

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
void printServerList(sorted_completions *compls, int block_type, struct node block) {
    server *current = compls->block_heads[block_type];
    printf("\n-- Blocco #%d #jobInQueue: %d--\n", block_type, block.jobInQueue);
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

// Inserisce un elemento nella lista ordinata
int insertSorted(sorted_completions *compls, server key) {
    printf("inserting sorted: %f\n", key.completion);

    int i;
    int n = compls->num_completion;

    for (i = n - 1; (i >= 0 && (compls->sorted[i].completion > key.completion)); i--) {
        compls->sorted[i + 1] = compls->sorted[i];
    }
    compls->sorted[i + 1] = key;
    compls->num_completion++;

    return (n + 1);
}

// Ricerca binaria di un elemento su una lista ordinata
int binarySearch(sorted_completions *compls, int low, int high, server key) {
    if (high < low) {
        return -1;
    }

    int mid = (low + high) / 2;
    if (key.completion == compls->sorted[mid].completion) {
        return mid;
    }
    if (key.completion > compls->sorted[mid].completion) {
        return binarySearch(compls, (mid + 1), high, key);
    }
    return binarySearch(compls, low, (mid - 1), key);
}

/* Function to delete an element */
int deleteElement(sorted_completions *compls, server key) {
    // Find position of element to be deleted
    printf("Deleting Sorted: %f\n", key.completion);

    int n = compls->num_completion;

    int pos = binarySearch(compls, 0, n - 1, key);

    if (pos == -1) {
        printf("Element not found");
        return n;
    }

    // Deleting element
    int i;
    for (i = pos; i < n; i++) {
        compls->sorted[i].completion = compls->sorted[i + 1].completion;
    }
    compls->num_completion--;

    return n - 1;
}

/*
char *format_server(server s) {
    char formatted[256];
    sprintf(&formatted,"");
}
*/

void print_array(sorted_completions *sorted, int num) {
    printf("List Status: %d | {", sorted->num_completion);

    for (int i = 0; i < num; i++) {
        printf("%f , ", sorted->sorted[i].completion);
    }
    printf("}");
}
