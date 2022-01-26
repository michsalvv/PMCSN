enum node_type getDestination(enum node_type from);

typedef struct {
    double value;
    server *server;
} completion;

typedef struct {
    completion sorted[TOTAL_SERVERS];
    int num_completion;
} sorted_completions;

server *iterateOver(server *s);
double min(double a, double b);
void printServerList(struct node b);
void waitInput();
void clearScreen();
int insertSorted(sorted_completions *compls, completion key);
