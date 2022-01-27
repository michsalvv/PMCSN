int getDestination(enum block_types from);

server *iterateOver(server *s);
double min(double a, double b);
int routing_from_temperature();
void waitInput();
void clearScreen();
int binarySearch(sorted_completions *compls, int low, int high, compl completion);
int insertSorted(sorted_completions *compls, compl completion);
int deleteElement(sorted_completions *compls, compl completion);
void print_completions_status(sorted_completions *server_list, struct block blocks[], int dropped, int completions);
