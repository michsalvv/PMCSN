#include <stdbool.h>

#define NUM_BLOCKS 5

#define MAX_SERVERS 50

#define START 0.0
#define DEBUG true

#define BUSY 1
#define IDLE 0

#define ONLINE 1
#define OFFLINE 0

#define USED 1
#define NOTUSED 0

#define NUM_REPETITIONS 128

// Input Values
#define LAMBDA_1 0.405556
#define LAMBDA_2 0.829545
#define LAMBDA_3 0.365

// Time Slot Values
#define TIME_SLOT_1 10800
#define TIME_SLOT_2 39600
#define TIME_SLOT_3 18000

// Services Time
#define SERV_TEMPERATURE_CTRL 15  //u = 0.06666667
#define SERV_TICKET_BUY 90
#define SERV_SEASON_GATE 10
#define SERV_TICKET_GATE 25
#define SERV_GREEN_PASS 30

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

// Numero di ripetizioni e batch
#define NUM_REPETITIONS 128
#define BATCH_B 1024
#define BATCH_K 128

#define handle_error(msg)   \
    do {                    \
        perror(msg);        \
        exit(EXIT_FAILURE); \
    } while (0)

enum block_types {
    TEMPERATURE_CTRL,
    TICKET_BUY,
    SEASON_GATE,
    TICKET_GATE,
    GREEN_PASS,
    EXIT
};

// Data Structures
// --------------------------------------------------------------------------------------------------

// Struttura che mantiene il clock
struct clock_t {
    double current;  // Tempo attuale di simulazione
    double next;     // Tempo attuale del prossimo evento, sia arrivo che completamento
    double arrival;  // Tempo attuale del prossimo arrivo
    double batch_current;
};

// Struttura che mantiene la somma accumulata
struct sum {
    double service;  // Tempi di servizio
    long served;     // Numero di job serviti
};

// Struttura che mantiene un job. Il puntatore *next implementa la Linked List
struct job {
    double arrival;
    struct job *next;
};

// Servente
typedef struct server_t {
    int id;
    int online;  // {0=OFFLINE, 1=ONLINE}
    int status;  // {0=IDLE, 1=BUSY}
    int used;    // {0=NOTUSED, 1=USED}     Utilizzata per print_statistic
    int stream;
    struct block *block;
    struct sum sum;
    bool need_resched;
    double time_online;
    double last_online;
} server;

typedef struct {
    server server_list[NUM_BLOCKS][MAX_SERVERS];
    int num_online_servers[NUM_BLOCKS];
    int time_slot;

} network_status;

struct area {
    double node;    /* time integrated number in the node  */
    double queue;   /* time integrated number in the queue */
    double service; /* time integrated number in service */
};

// Blocco
struct block {
    struct job *head_service;
    struct job *tail;
    struct job *head_queue;
    double active_time;
    int jobInQueue;
    int jobInBlock;

    int batch_block;
    int batch_queue;
    enum block_types type;

    int batch_arrivals;
    int total_arrivals;
    int total_completions;
    int total_bypassed;  // Utilizzato per il blocco GREEN_PASS
    double service_rate;
    struct area area;
};

// Struttura che mantiene un completamento su un server
typedef struct {
    server *server;
    double value;
} compl ;

// Struttura che mantiene la lista ordinata di tutti i completamenti
typedef struct {
    compl sorted_list[NUM_BLOCKS * MAX_SERVERS];
    int num_completions;
} sorted_completions;

typedef struct {
    int slot_config[3][NUM_BLOCKS];
} network_configuration;
// --------------------------------------------------------------------------------------------------