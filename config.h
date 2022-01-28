#define NUM_BLOCKS 5

#define MAX_SERVERS 50

#define START 0.0
#define DEBUG 0

#define BUSY 1
#define IDLE 0

#define ONLINE 1
#define OFFLINE 0

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
    SEASON_GATE,
    TICKET_GATE,
    GREEN_PASS,
    EXIT
};

// Struttura che mantiene la somma accumulata
struct sum {
    double service;  // Tempi di servizio
    long served;     // Numero di job serviti
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
    int online;  // {0=OFFLINE, 1=ONLINE}
    int status;  // {0=IDLE, 1=BUSY}
    int stream;
    struct block *block;
    struct sum sum;
    bool need_resched;
} server;

typedef struct {
    server server_list[NUM_BLOCKS][MAX_SERVERS];
    int num_online_servers[NUM_BLOCKS];

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

    //struct job in_service;
    //struct job *tail_second;
    // double opening_time;
    double active_time;
    int jobInQueue;
    int jobInBlock;
    enum block_types type;

    int total_arrivals;
    int total_completions;
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