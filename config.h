#define TEMP_SERVERS 3
#define TICKET_SERVERS 4
#define START 0.0
#define STOP 18000.0
#define NUM_SERVICES 2;     // TEMP e TICKETS


#define handle_error(msg)                                                      \
	do {                                                                   \
		perror(msg);                                                   \
		exit(EXIT_FAILURE);                                            \
	} while (0)

enum node_type { TEMP, TICKETS };
