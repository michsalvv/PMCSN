#define TEMP_SERVERS 3
#define TICKET_SERVERS 4
#define START 0.0
#define STOP 18000.0
#define NUM_SERVICES 2;  // TEMP e TICKETS

#define START 0.0
#define STOP 10800.0  // Configurazone per prima fascia
#define SERVERS 3
#define DEBUG 0

#define handle_error(msg)   \
    do {                    \
        perror(msg);        \
        exit(EXIT_FAILURE); \
    } while (0)

enum node_type {
    TEMPERATURE_CTRL,
    TICKETS_BUY,
    TICKETS_GATE,
    SEASON_GATE,
    GREEN_PASS
};
