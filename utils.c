#include <math.h>
#include <stdio.h>

#include "config.h"

enum node_type
getDestination(enum node_type from) {
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
void printServerList(network_status *compls, int block_type, struct node block) {
    printf("asdadasd");
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

// Ricerca binaria di un elemento su una lista ordinata
int binarySearch(network_status *compls, int low, int high, server *key) {
    if (high < low) {
        return -1;
    }

    int mid = (low + high) / 2;
    if (key->completion == compls->server_list[mid]->completion) {
        return mid;
    }
    if (key->completion > compls->server_list[mid]->completion) {
        return binarySearch(compls, (mid + 1), high, key);
    }
    return binarySearch(compls, low, (mid - 1), key);
}

// Inserisce un elemento nella lista ordinata
int insertSorted(network_status *compls, server *key) {
    printf("inserting server_list: %f\n", key->completion);

    int i;
    int n = compls->sorted_completions.num_completion;

    for (i = n - 1; (i >= 0 && (compls->server_list[i]->completion > key->completion)); i--) {
        compls->sorted_completions.sorted_ids[i + 1] = compls->sorted_completions.sorted_ids[i];
    }
    compls->sorted_completions.sorted_ids[i + 1] = key->id;
    compls->sorted_completions.num_completion++;

    return (n + 1);
}

/* Function to delete an element */
int deleteElement(network_status *compls, server *key) {
    // Find position of element to be deleted
    printf("Deleting Sorted: %f\n", key->completion);

    int n = compls->sorted_completions.num_completion;

    int pos = binarySearch(compls, 0, n - 1, key);

    if (pos == -1) {
        printf("Element not found");
        return n;
    }

    // Deleting element
    int i;
    server *saved = compls->server_list[pos];
    for (i = pos; i < n; i++) {
        compls->server_list[i] = compls->server_list[i + 1];
    }
    compls->server_list[n + 1] = saved;
    compls->server_list[n + 1]->status = IDLE;
    compls->server_list[n + 1]->completion = INFINITY;
    compls->sorted_completions.num_completion--;

    return n - 1;
}

/*
char *format_server(server s) {
    char formatted[256];
    sprintf(&formatted,"");
}
*/

void print_array(network_status *server_list, int num) {
    printf("List Status: %d | {", server_list->sorted_completions.num_completion);

    for (int i = 0; i < num; i++) {
        printf("%d , ", server_list->sorted_completions.sorted_ids[i]);
    }
    printf("}\n");
}
