#include <math.h>
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

//TODO rifare stampando solo i completions
// Stampa la lista dei server con il relativo stato
/*
void printServerList(network_status *compls, int block_type, struct node block) {
    server *current = compls->block_heads[block_type];
    printf("\n-- Blocco #%d #jobInQueue: %d--\n", block_type, block.jobInQueue);
    while (current != NULL) {
        printf("Server #%d \tStatus: %d\tCompletion: %f\t\t Type: %d\tStream: %d\n", current->id, current->status, current->completion, current->nodeType, current->stream);
        if (current->next == NULL) break;
        current = current->next;
    }
}
*/

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

/* Function to delete an element */
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
    compls->num_completions--;

    return n - 1;
}

/*
char *format_server(server s) {
    char formatted[256];
    sprintf(&formatted,"");
}
*/

void print_array(sorted_completions *compls, int num) {
    printf("List Status: %d | {", compls->num_completions);

    for (int i = 0; i < num; i++) {
        printf("%f , ", compls->sorted_list[i].value);
    }
    printf("}\n");
}
