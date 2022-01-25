#include "config.h"

// Lista degli eventi relativi ad ogni servente di un centro
typedef struct {
    double t;    // Tempo del next-event
    int status;  // Stato dell'evento: {1=BUSY, 0=IDLE}
} event_list[SERVERS + 1];

// Struttura che mantiene il clock
struct {
    double current;  // Tempo attuale di simulazione
    double next;     // Tempo attuale del prossimo evento, sia arrivo che completamento
    double arrival;  // Tempo attuale del prossimo arrivo
} clock;

// Struttura che mantiene la somma accumulata
struct {
    double service;  // Tempi di servizio
    long served;     // Numero di job serviti
} sum[SERVERS + 1];

// Struttura che mantiene un job. Il puntatore *next implementa la Linked List
struct job {
    double arrival;
    struct job *next;
};

// Servente
struct server_t {
    int id;
    int status;  // {0=idle, 1=busy}
    double completion;
    int stream;
    enum node_type nodeType;
    struct server_t *next;
};
typedef struct server_t server;

// Blocco
struct node {
    struct job *head;
    struct job *tail;
    struct job in_service;
    struct job *head_second;
    struct job *tail_second;
    // struct area area;
    // double opening_time;
    double active_time;
    int number;
    enum node_type type;  // forse non serve

    int num_server;
    server *firstServer;

} blocks[2];