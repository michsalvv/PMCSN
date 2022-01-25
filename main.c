#include <stdio.h>
#include <stdlib.h>

#include "DES/rngs.h"
#include "DES/rvgs.h"
#include "config.h"
#include "structures.h"

int s, e;
long number = 0;  // number = #job in coda
long index = 0;   // #job in totale osservati
double area = 0.0;
event_list event;  // dimensione(lista) = deve contenere una entry per ogni tipo di evento --> m+1 entry, m=#server

/*
 * generate the next arrival time, with rate 1/2
 */
double GetArrival(void) {
    static double arrival = START;
    SelectStream(0);
    arrival += Exponential(0.346153833);
    return (arrival);
}

/*
 * generate the next service time, with rate 1/6
 */
double GetService(void) {
    SelectStream(1);
    return (Exponential(0.9));
}
/*
 * return the index of the next event type
 */
int NextEvent(event_list event) {
    int e;
    int i = 0;

    while (
        event[i].status ==
        0) /* find the index of the first 'active' element in the event list */
        i++;
    e = i;
    while (i < SERVERS) { /* now, check the others to find which event type is
                             most imminent */
        i++;
        if ((event[i].status == 1) && (event[i].t < event[e].t))
            e = i;
    }
    return (e);
}

/*
 * return the index of the available server idle longest
 */
int FindOne(event_list event) {
    int s;
    int i = 1;

    while (event[i].status ==
           1) /* find the index of the first available (idle) server*/
        i++;
    s = i;
    while (i < SERVERS) { /* now, check the others to find which has been idle
                             longest    */
        i++;
        if ((event[i].status == 0) && (event[i].t < event[s].t))
            s = i;
    }
    return (s);
}

int main() {
    PlantSeeds(0);

    // All'indice 0 c'è l'arrivo. I successivi m indici riguardano i server
    clock.current = START;
    event[0].t = GetArrival();  // Viene generato l'istante in cui si verificherà il primo arrivo
    event[0].status = 1;

    // Per ogni server genero un evento
    // Imposto ogni evento con tempo corrente a zero
    for (s = 1; s <= SERVERS; s++) {
        event[s].t = START;
        event[s].status = 0;  // server al tempo 0 sono in IDLE
        sum[s].service = 0.0;
        sum[s].served = 0;
    }

    while ((event[0].status != 0) || (number != 0)) {
        printEventList(event, number, clock.current);
        waitInput();
        e = NextEvent(event);     // ritorna il prossimo evento imminente
        clock.next = event[e].t;  // Salviamo il l'istante del prossimo evento individuato

        area += (clock.next - clock.current) * number; /* update integral  */
        clock.current = clock.next;                    // Facciamo "scorrere" il clock fino all'istante dell'evento individuato.

        printf("\nAvanzamento clock: = %f\n", clock.current);
        waitInput();

        // Se il prossimo evento individuato è all'indice 0 nell'event list, allora è un arrivo
        if (e == 0) {
            process_arrival();
        }
        // Se il prossimo evento individuato è all'indice !=0 nell'event list, allora è un completamento
        else {
            process_completion();
        }
        waitInput();
    }

    printf("\nfor %ld jobs the service node statistics are:\n\n", index);
    printf("  avg interarrivals .. = %6.2f\n", event[0].t / index);
    printf("  avg wait ........... = %6.2f\n", area / index);
    printf("  avg # in node ...... = %6.2f\n", area / clock.current);

    for (s = 1; s <= SERVERS; s++) /* adjust area to calculate */
        area -= sum[s].service;    /* averages for the queue   */

    printf("  avg delay .......... = %6.2f\n", area / index);
    printf("  avg # in queue ..... = %6.2f\n", area / clock.current);
    printf("\nthe server statistics are:\n\n");
    printf("    server     utilization     avg service        share\n");
    for (s = 1; s <= SERVERS; s++)
        printf("%8d %14.3f %15.2f %15.3f\n", s, sum[s].service / clock.current,
               sum[s].service / sum[s].served, (double)sum[s].served / index);
    printf("\n");

    return (0);
}

void process_arrival() {
    printf("--> Arrivo \n\n");
    number++;                   // Numero di job in coda
    event[0].t = GetArrival();  // Si simula il prossimo istante di arrivo
    printf("Generazione prossimo arrivo... %f\n", event[0].t);

    if (event[0].t > STOP)  // Fine simulazione, non si accettano più arrivi.
        event[0].status = 0;

    if (number <= SERVERS) {  // Se numero di job in servizio è minore del
        // numero di server allora c'è un server libero
        // (minore uguale perchè già è stato incrementato)
        double service = GetService();
        s = FindOne(event);
        sum[s].service += service;
        sum[s].served++;
        event[s].t = clock.current + service;
        event[s].status = 1;
        printf(
            "Ci sono serventi liberi quindi è possibile generare un tempo di "
            "servizio... %f\n",
            service);
        printf(
            "Il completamento avverrà all'istante: (%f + %f) = %f, dal "
            "servente #%d\n",
            clock.current, service, event[s].t, s);
    }
}

void process_completion() {
    printf("--> Completamento\n\n");
    index++;
    number--;
    s = e;
    if (number >=
        SERVERS) {  // Se c'è un job in coda ma non ci sono serventi liberi
        printf("Non ci sono server liberi\n");
        double service = GetService();
        sum[s].service += service;
        sum[s].served++;
        event[s].t = clock.current + service;
        printf("Generazione tempo di servizio... %f\n", service);
    } else {
        printf("Completamento avvenuto\n");
        event[s].status = 0;
    }
}