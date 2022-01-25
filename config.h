#define TEMPERATURE_CTRL_SERVERS 3
#define TICKET_BUY_SERVERS 4
#define TICKET_GATE_SERVERS 5
#define SEASON_GATE_SERVERS 5
#define GREEN_PASS_SERVERS 5

#define START 0.0
#define STOP 10800.0  // Configurazone per prima fascia
#define SERVERS 3
#define DEBUG 0

#define BUSY 1
#define IDLE 0

#define handle_error(msg)   \
    do {                    \
        perror(msg);        \
        exit(EXIT_FAILURE); \
    } while (0)

enum node_type {
    TEMPERATURE_CTRL,
    TICKET_BUY,
    TICKET_GATE,
    SEASON_GATE,
    GREEN_PASS
};

#define NUM_BLOCKS 5

// Struttura che mantiene il clock
struct clock_t {
    double current;  // Tempo attuale di simulazione
    double next;     // Tempo attuale del prossimo evento, sia arrivo che completamento
    double arrival;  // Tempo attuale del prossimo arrivo
};

// Struttura che mantiene la somma accumulata
// struct {
//     double service;  // Tempi di servizio
//     long served;     // Numero di job serviti
// } sum[SERVERS + 1];

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
    int jobInQueue;
    enum node_type type;  // forse non serve

    int num_server;
    server *firstServer;
};