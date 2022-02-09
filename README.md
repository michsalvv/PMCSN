# Progetto PMCSN - Modellistica, simulazione e valutazione delle prestazioni
Quest progetto permette di simulare una stazione ferroviaria, al fine di valutare il numero ottimo di serventi per gestire una giornata lavorativa di 19 ore.

- La cartella ```base``` contiene il programma simulativo per il caso base.
- La cartella ```migliorativo``` contiene il programma simulativo per l'algoritmo migliorativo.
- La cartella ```statistiche``` contiene i programmi per generare le statistiche medie dai csv prodotti nelle simulazioni.

## Istruzioni
- Spostarsi sulla cartella ```base``` o ```migliorativo``` al seconda del caso d'uso che si vuole simulare
- Impostare in ```conf.h``` i parametri del sistema desiderati
- Impostare in ```main.c``` nella funzione ```init_config()``` la configurazione di serventi che si vuole testare
- Compilare l'eseguibile tramite il comando ```make```
- Eseguire il programma con il comando ```./simulate-[base/migliorativo] \<MODE> \<SLOT>``` 
- Recuperare i risultati dalla cartella ```results/<MODE>``` ed eventualmente valutare le statistiche tramite i programmi presenti in ```/statistiche```

    - ```uvs < results.csv```: valuta la media e la varianza 
    - ```estimate < results.csv```:valuta la media e l'intervallo di confidenza al 95%
    - ```acs < results.csv```: valuta l'autocorrelazione del campione

