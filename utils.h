int getDestination(enum block_types from);

server *iterateOver(server *s);
double min(double a, double b);
int routing_from_temperature();
void waitInput();
void clearScreen();
int binarySearch(sorted_completions *compls, int low, int high, compl completion);
int insertSorted(sorted_completions *compls, compl completion);
int deleteElement(sorted_completions *compls, compl completion);
void print_block_status(sorted_completions *server_list, struct block blocks[], int dropped, int completions, int bypassed);
void print_statistics(network_status *network, struct block blocks[], double currentClock, sorted_completions *server_list);
static inline char *stringFromEnum(enum block_types f);
network_configuration get_config(int *values_1, int *values_2, int *values_3);
void print_network_status(network_status *network);
double print_cost_details(network_configuration conf);
void print_percentage(double part, double total, double oldPart);
