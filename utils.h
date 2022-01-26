enum node_type getDestination(enum node_type from);

typedef struct {
    server sorted[TOTAL_SERVERS];
    int num_completion;
} sorted_completions;

server *iterateOver(server *s);
double min(double a, double b);
void printServerList(struct node b);
void waitInput();
void clearScreen();
int binarySearch(sorted_completions *compls, int low, int high, server key);
int insertSorted(sorted_completions *compls, server key);
int deleteElement(sorted_completions *compls, server key);
void print_array(sorted_completions sorted, int num);
