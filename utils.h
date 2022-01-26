enum node_type getDestination(enum node_type from);

typedef struct {
    server sorted[TOTAL_SERVERS];
    int num_completion;
    server *block_heads[5];
} sorted_completions;

server *iterateOver(server *s);
double min(double a, double b);
void printServerList(sorted_completions *compls, int block_type, struct node block);
void waitInput();
void clearScreen();
int binarySearch(sorted_completions *compls, int low, int high, server key);
int insertSorted(sorted_completions *compls, server key);
int deleteElement(sorted_completions *compls, server key);
void print_array(sorted_completions *sorted, int num);
