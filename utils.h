enum node_type getDestination(enum node_type from);

server *iterateOver(server *s);
double min(double a, double b);
void printServerList(network_status *compls, int block_type, struct node block);
void waitInput();
void clearScreen();
int binarySearch(sorted_completions *compls, int low, int high, compl completion);
int insertSorted(sorted_completions *compls, compl completion);
int deleteElement(sorted_completions *compls, compl completion);
void print_array(sorted_completions *server_list, int num);
