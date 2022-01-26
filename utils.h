enum node_type getDestination(enum node_type from);

server *iterateOver(server *s);
double min(double a, double b);
void printServerList(network_status *compls, int block_type, struct node block);
void waitInput();
void clearScreen();
int binarySearch(network_status *compls, int low, int high, server key);
int insertSorted(network_status *compls, server *key);
int deleteElement(network_status *compls, server *key);
void print_array(network_status *server_list, int num);
