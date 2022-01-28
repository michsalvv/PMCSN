#define NUM_BLOCKS 5

#define TEMPERATURE_CTRL_SERVERS 3
#define TICKET_BUY_SERVERS 20
#define SEASON_GATE_SERVERS 1
#define TICKET_GATE_SERVERS 5
#define GREEN_PASS_SERVERS 15
#define TOTAL_SERVERS TEMPERATURE_CTRL_SERVERS + TICKET_BUY_SERVERS + TICKET_GATE_SERVERS + SEASON_GATE_SERVERS + GREEN_PASS_SERVERS

#define START 0.0
#define STOP 10800.0  // Configurazone per prima fascia
#define SERVERS 3
#define DEBUG 0

#define BUSY 1
#define IDLE 0

// Input Values
#define LAMBDA_1 2.888889
#define LAMBDA_2 4.242424
#define LAMBDA_3 2.266667

// Time Slot Values
#define TIME_SLOT_1 10800
#define TIME_SLOT_2 39600
#define TIME_SLOT_3 18000

// Services Time
#define SERV_TEMPERATURE_CTRL 0.9
#define SERV_TICKET_BUY 18
#define SERV_TICKET_GATE 4
#define SERV_SEASON_GATE 2.5
#define SERV_GREEN_PASS 10

// Routing Probabilities
#define P_EXIT_TEMP 0.2
#define P_TICKET_BUY 48.295
#define P_TICKET_GATE 26.005
#define P_SEASON_GATE 25.5
#define P_EXIT_GREEN 0.05

// Costi mensili per server
#define CM_TEMPERATURE_CTRL_SERVER 300
#define CM_TICKET_BUY_SERVER 200
#define CM_SEASON_GATE_SERVER 50
#define CM_TICKET_GATE_SERVER 1300
#define CM_GREEN_PASS_SERVER 800

#define handle_error(msg)   \
    do {                    \
        perror(msg);        \
        exit(EXIT_FAILURE); \
    } while (0)

enum block_types {
    TEMPERATURE_CTRL,
    TICKET_BUY,
    TICKET_GATE,
    SEASON_GATE,
    GREEN_PASS,
    EXIT
};

// Struttura che mantiene il clock
struct clock_t {
    double current;  // Tempo attuale di simulazione
    double next;     // Tempo attuale del prossimo evento, sia arrivo che completamento
    double arrival;  // Tempo attuale del prossimo arrivo
};

// Struttura che mantiene un job. Il puntatore *next implementa la Linked List
struct job {
    double arrival;
    struct job *next;
};

// Servente
typedef struct server_t {
    int id;
    int status;  // {0=IDLE, 1=BUSY}
    int stream;
    enum block_types block_type;
    struct server_t *next;
} server;

// Blocco
struct block {
    struct job *head;
    struct job *tail;
    struct job in_service;
    struct job *head_second;
    struct job *tail_second;
    double area;
    // double opening_time;
    double active_time;
    int jobInQueue;
    int jobInBlock;
    enum block_types type;  // forse non serve

    int total_arrivals;
    int total_completions;

    int num_server;
    server *firstServer;
};

// Struttura che mantiene un completamento su un server
typedef struct {
    server *server;
    double value;
} compl ;

// Struttura che mantiene la lista ordinata di tutti i completamenti
typedef struct {
    compl sorted_list[TOTAL_SERVERS];
    int num_completions;
} sorted_completions;

// Struttura che mantiene la somma accumulata
// struct {
//     double service;  // Tempi di servizio
//     long served;     // Numero di job serviti
// } sum[SERVERS + 1];

// struct area {
//     double node;    /* time integrated number in the node  */
//     double queue;   /* time integrated number in the queue */
//     double service; /* time integrated number in service */
// };