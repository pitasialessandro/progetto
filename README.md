# Progetto Ridotto Laboratorio II Università di Pisa 24/25
*Il progetto si divide in 2 macro sezioni:*
1. **Costruzione del grafo degli attori**
A partire dai file 'name.basics.tsv title.principals.tsv'  
'CreaGrafo.java' produce in output un grafo che evidenzia tutti gli attori _j_ che hanno partecipato in almeno un film o un altro evento con l'attore _i_.

2. **Calcolo dei cammini minimi tra gli attori**
Dati in input i file 'nomi.txt grafo.txt' il programma 'cammini.c' fa parsing dei due file, li organizza in strutture dati opportune e successivamente effettua il calcolo dei cammini minimi tra gli attori forniti sulla pipe _cammini.pipe_ dal codice 'cammini.py', scrivendo il risultato negli opportuni file *a.b* dove a e b rappresentano il codice degli attori di partenza e di destinazione.
---
## Svolgimento del progetto
### 1. Dettaglio del Parsing delle Righe del File `name.basics.tsv`

Il programma Java `CreaGrafo` si occupa del parsing del file `name.basics.tsv` per estrarre le informazioni sugli attori rilevanti.

#### Fase di Lettura e Filtraggio
Il file `name.basics.tsv` viene letto riga per riga utilizzando un `BufferedReader`. La prima riga, che contiene le intestazioni, viene scartata. Per ogni riga successiva, il testo viene diviso in campi utilizzando il carattere di tabulazione (`\t`) come delimitatore tramite il metodo `split("\t")`.

Durante il parsing, vengono applicati i seguenti filtri:
* Vengono considerate solo le righe che hanno almeno 5 campi per garantire la validità del record.
* Le righe con anno di nascita mancante (`\N`) vengono ignorate.
* Vengono inclusi solo gli individui la cui professione contiene "actor" o "actress".

#### Estrazione e Conversione dei Dati
Una volta superati i filtri, i campi rilevanti vengono estratti:
* Il **codice attore** (es. `nm1234567`) viene parsato rimuovendo il prefisso "nm" e convertito in un intero.
* Il **nome dell'attore** viene estratto direttamente.
* L'**anno di nascita** viene estratto e convertito in un intero.

#### Memorizzazione dei Dati Parsati
Gli attori validi vengono memorizzati in una `HashMap<Integer, Attore>`. La chiave della mappa è il codice intero dell'attore, permettendo un accesso efficiente per successivi lookup. La classe `Attore` (definita come inner static class) incapsula `codice`, `nome`, `anno` e un `Set<Integer>` per i coprotagonisti, inizialmente vuoto.

```java
// Estratto del parsing e filtraggio in CreaGrafo.java
try (BufferedReader br = new BufferedReader(new FileReader(args[0]))) {
    String linea = br.readLine(); // Scarta la 1a linea (intestazioni)
    while((linea = br.readLine()) != null) {
        String[] campi = linea.split("\t");

        if (campi.length < 5) continue; 

        String birthYear = campi[2];
        String profession = campi[4];

        if (birthYear.equals("\\N")) continue; 
        if (!profession.contains("actor") && !profession.contains("actress")) continue;

        int codice = Integer.parseInt(campi[0].substring(2)); 
        String nome = campi[1];
        try {
            int anno = Integer.parseInt(birthYear);
            attori.put(codice, new Attore(codice, nome, anno));
        } catch (NumberFormatException e) {
            continue; 
        }
    }
}
```

### 2. Implementazione della Coda FIFO per l'algoritmo BFS
La coda è implementata come una lista di nodi (`queue_node`), contenente due puntatori `front` e `rear` che permettono le operazioni di inserimento e rimozione dalla coda in tempo costante.

```C
// Nodo per coda BFS: memorizza INDICI nell'array attori
typedef struct {
    int attore_idx; // Indice dell'attore nell'array attori
    int from_idx;   // Indice del padre nell'array attori
    struct queue_node *next;
} queue_node;

// Coda FIFO per BFS
typedef struct {
    queue_node *front;
    queue_node *rear;
} queue;
```
### Informazioni memorizzate in ogni elemento della coda (`queue_node`)
- `attore_idx` : **l'indice** dell'attore corrente all'interno dell'array `attori` caricato in memoria, l'utilizzo dell'indice anzichè del codice attore permette un accesso diretto all'array.

- `from_idx`: l'indice dell'attore **padre** nell'array `attori`. Questo campo è necessario per ricostruire il cammino a ritroso una volta trovato il cammino minimo dalla sorgente alla destinazione.

- `next`: il puntatore al nodo successivo nella coda, inizializzato a `NULL`.

### 3. Dettaglio di Come Vengono Ricostruiti i Nodi Intermedi del Cammino Minimo in `cammini.c`
La ricostruzione del cammino minimo avviene dopo che l'algoritmo BFS ha trovato l'attore di destinazione. Questo processo si basa sulle informazioni dei nodi padre (`from_idx`) memorizzate durante la fase di esplorazione.

### Registrazione dei Nodi Padre
Durante l'esecuzione dell'algoritmo BFS, ogni volta che un nodo next_idx viene visitato per la prima volta (ovvero, viene aggiunto alla coda) a partire da un nodo curr_idx, l'indice del padre (curr_idx) viene immediatamente registrato. Questa registrazione avviene in un array globale parent_indices, la cui dimensione è pari al numero totale degli attori.

In particolare, l'elemento parent_indices[next_idx] viene impostato a curr_idx. Inizialmente, tutti gli elementi di parent_indices sono impostati a -1 per indicare che nessun nodo è stato ancora visitato o che è la sorgente del cammino.

```C

// Array per memorizzare l'indice del padre per ogni indice di attore
int *parent_indices = calloc(tota, sizeof(int));
// ...
// Inizializza l'array dei padri a -1
for (int i = 0; i < tota; i++) parent_indices[i] = -1; 

// Durante la BFS, all'interno del loop di esplorazione dei vicini:
parent_indices[next_idx] = curr_idx; // Salva l'indice del padre per il nodo figlio
```
### Processo di Backtracking
Una volta che l'attore di destinazione (`dst_idx`) viene raggiunto dall'algoritmo BFS, si avvia il processo di backtracking per ricostruire il cammino:

- Viene inizializzato un array `path_indices`, che conterrà gli indici degli attori nel cammino.

- Si entra in un ciclo while che continua finché `curr_path_idx` non è uguale a -1 (il valore sentinel che indica la sorgente o l'assenza di un padre).

- Ad ogni iterazione, il valore di `curr_path_idx` viene aggiunto all'array path_indices.

- Successivamente, `curr_path_idx` viene aggiornato con il valore parent_indices[curr_path_idx], risalendo così all'attore padre nel cammino.

Quando il ciclo termina, l'array path_indices contiene gli indici degli attori che formano il cammino minimo in ordine inverso.

```C
// Backtracking del cammino e stampa
int path_indices[tota]; // Array temporaneo per gli indici del cammino
int len = 0, curr_path_idx = dst_idx; // Inizia dall'indice di destinazione

// Risali dal nodo di destinazione al nodo sorgente
while (curr_path_idx != -1) {
    path_indices[len++] = curr_path_idx;
    curr_path_idx = parent_indices[curr_path_idx];
}

// Stampa il cammino al contrario (dal sorgente alla destinazione)
for (int i = len - 1; i >= 0; i--) { 
    attore *act = &attori[path_indices[i]]; // Accesso diretto tramite indice all'attore
    fprintf(out, "%d\t%s\t%d\n", act->codice, act->nome, act->anno);
}
```
### 4. Dettaglio di Come il Thread Gestore di Segnali Comunica al Programma Principale di Interrompere l'Elaborazione
Il progetto utilizza un thread dedicato alla gestione dei segnali per intercettare il segnale `SIGINT` e comunicare al programma principale la richiesta di terminazione in modo controllato.

### Blocco dei Segnali (Masking)
All'avvio del main thread, il segnale SIGINT  viene bloccato per tutti i thread del processo tramite la funzione`pthread_sigmask`,garantendo che il segnale sia "pending" fino a quando non viene esplicitamente gestito dal thread designato.

### Ruolo del Thread Gestore di Segnali
Il thread apposito, signal_handler_thread, viene creato con una maschera di segnali che gli consente di ricevere SIGINT. Questo thread entra in un ciclo infinito e attende in modo sincrono la ricezione di un segnale specifico usando la funzione `sigwait(args->sigset, &sig)`. sigwait sospende l'esecuzione del thread finché uno dei segnali presenti nella maschera non viene ricevuto, rendendo la gestione dei segnali prevedibile e sicura in un ambiente multithread.

### Comunicazione Tramite Variabili Atomiche
La comunicazione e la sincronizzazione tra il signal_handler_thread e il main thread avvengono attraverso l'uso di due variabili atomic_bool condivise, passate come puntatori all'interno della struttura signal_thread_args_t:

```atomic_bool *pipe_phase```: Questa variabile atomica indica la fase corrente dell'esecuzione del programma principale:

- Se il programma si trova nella fase di costruzione del grafo o di lettura iniziale dei file, un SIGINT intercettato dal gestore fa sì che venga stampato un messaggio di avviso ("Costruzione del grafo in corso"). In questa fase, il programma non termina.

- Se il programma è entrato nella fase di lettura dalla pipe cammini.pipe e di elaborazione delle richieste BFS, un SIGINT viene interpretato come una richiesta esplicita di terminazione.

alla ricezione di SIGINT, prima di terminarsi, il thread signal_handler_thread imposta atomic_store(args->terminate_program, true). Questo flag segnala al main thread che è stata richiesta la terminazione dell'elaborazione principale.

Il main thread, nel suo loop principale di lettura dalla pipe, controlla periodicamente il valore della flag, se restituisce true, il loop si interrompe, e il main procede con le operazioni di cleanup e la terminazione del programma.

```C
// Nel signal_handler_thread
if (sig == SIGINT) {
    if (!atomic_load(args->pipe_phase)) {
        // ... stampa messaggio di "Costruzione in corso" su stderr
    } else {
        atomic_store(args->terminate_program, true); // Segnala al main di terminare
        pthread_exit(NULL); // Il signal handler termina la sua esecuzione
    }
}

// Nel main thread
atomic_bool main_terminate_flag = ATOMIC_VAR_INIT(false);
atomic_bool main_pipe_phase = ATOMIC_VAR_INIT(false);

// ...

// Si imposta main_pipe_phase a true quando si entra nella fase di lettura dalla pipe
atomic_store(&main_pipe_phase, true);

// Loop principale di elaborazione richieste dalla pipe
while (1) {
    if (atomic_load(&main_terminate_flag)) {
        fprintf(stderr, "Terminazione richiesta dal signal handler\n");
        break; // Esce dal loop e procede alla terminazione
    }
    // ...
```
---