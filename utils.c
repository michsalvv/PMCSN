#include "config.h"

enum node_type getDestination(enum node_type from){
    switch (from)
    {
    case TEMP:
        // TODO aggiungere probabilità
        return TICKETS;
        break;
    
    default:
        break;
    }
}