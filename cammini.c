#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <semaphore.h>
#include <stdatomic.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdint.h>
#include <sys/times.h>

#define BUFFER_CAPACITY 100

typedef struct {
  int codice;
  char *nome;
  int anno;
  int numcop;
  int *cop;
} attore;

typedef struct {
    char **buffer_linee;
    int capacity;
    int count;
    int in;
    int out;
    
    attore *attori;
    int num_attori;

    pthread_mutex_t mutex;
    sem_t emptySlots;
    sem_t fullSlots;
    atomic_bool finito;
} shared_buffer_t;

typedef struct {
    int pipe_fd;
    sigset_t *sigset;
    atomic_bool *terminate_program;
    atomic_bool *pipe_phase;
} signal_thread_args_t;

typedef struct {
    attore *attori;
    int tota;
    int a;
    int b;
} bfs_args_t;


void termina(const char *messaggio);
void cleanup(shared_buffer_t *sb, attore *attori, int num_attori, int pipe_fd);
void *bfs_thread(void *args);

void *signal_handler_thread(void *arg) {
  // anche questo thread ha i segnali bloccati, ma aspetta SIGINT con la wait
    signal_thread_args_t *args = (signal_thread_args_t *)arg;

    printf("%d\n", getpid());
    fflush(stdout);

    while (1) {
        int sig;
        // attendi in modo sincrono un segnale tra quelli del set
        if (sigwait(args->sigset, &sig) != 0) {
            perror("erorre nella sigwait");
            pthread_exit(NULL);
        }

        if (sig == SIGINT) {
            if (!atomic_load(args->pipe_phase)) {
                int x;
                const char msg[] = "Costruzione del grafo in corso\n";
                if ((x = write(STDERR_FILENO, msg, sizeof(msg) - 1)) != sizeof(msg) -1)
                    termina("errore write");
            } else {
                fprintf(stderr, "SIGINT ricevuto nella fase pipe. Terminazione...\n");
                atomic_store(args->terminate_program, true);
                pthread_exit(NULL);
            }
        }
    }
}

attore crea_attore(int codice, const char *nome, int anno) {
    attore a;
    a.codice = codice;
    a.nome = strdup(nome);
    if (!a.nome)
        termina("Errore nella strdup");
    a.anno = anno;
    a.numcop = 0;
    a.cop = NULL;
    return a;
}

void free_attore(attore *a) {
    if (a->nome) free(a->nome);
    if (a->cop) free(a->cop);
}

void free_array_attori(attore *attori, int num_attori) {
    if (attori == NULL) return;
    for (int i = 0; i < num_attori; i++)
        free_attore(&attori[i]);
    free(attori);
}

int carica_attori(const char *nomefile, attore **attori) {
    FILE *f = fopen(nomefile, "r");
    if (!f) return -1;

    char *buffer = NULL;
    int capacity = 100;
    size_t n = 0;
    ssize_t e;

    attore *lista = malloc(sizeof(attore) * capacity);
    if (!lista) {
        fclose(f);
        return -1;
    }
    int count = 0;

    while ((e = getline(&buffer, &n, f)) != -1) {
        char *codice_str = strtok(buffer, "\t");
        char *nome = strtok(NULL, "\t");
        char *anno_str = strtok(NULL, "\t\n");

        if (!codice_str || !nome || !anno_str) {
            fprintf(stderr, "skipped a line\n");
            continue;
        }
        
        int anno = atoi(anno_str);
        int codice = atoi(codice_str);
        if (count == capacity) {
            capacity *= 2;
            attore *tmp = realloc(lista, sizeof(attore) * capacity);
            if (!tmp) {
                free_array_attori(lista, count);
                free(buffer);
                fclose(f);
                return -1;
            }
            lista = tmp;
        }
        
        lista[count++] = crea_attore(codice, nome, anno);
    }

    free(buffer);
    fclose(f);

    *attori = lista;
    fprintf(stderr, "numero attori: %d\n", count);
    return count;
}

int confronta_attori(const void *a, const void *b) {
    return ((attore *)a)->codice - ((attore *)b)->codice;
}

void *consumer(void *arg) {
    shared_buffer_t *sb = (shared_buffer_t *)arg;
    char *token;

    while (true) {
        // attendo che ci sia almeno un elemento nel buffer o il producer abbia finito
        sem_wait(&sb->fullSlots);
        pthread_mutex_lock(&sb->mutex);

        // se buffer vuoto e producer ha finito, esco dal ciclo
        if (sb->in == sb->out && atomic_load(&sb->finito)) {
            pthread_mutex_unlock(&sb->mutex);
            // Rimette sem_post per altri consumer eventualmente bloccati
            sem_post(&sb->fullSlots);
            break;
        }

        // estraggo la linea dal buffer
        char *line = strdup(sb->buffer_linee[sb->out]);
        free(sb->buffer_linee[sb->out]);
        sb->out = (sb->out + 1) % sb->capacity;
        pthread_mutex_unlock(&sb->mutex);
        sem_post(&sb->emptySlots);

        // parsing riga
        char *saveptr;
        if ((token = strtok_r(line, "\t", &saveptr)) == NULL) {
            free(line);
            continue;
        }
        int codice_attore = atoi(token);

        if ((token = strtok_r(NULL, "\t", &saveptr)) == NULL) {
            free(line);
            continue;
        }
        int numcop = atoi(token);

        // alloca array coprotagonisti
        int *cop = malloc(sizeof(int) * numcop);
        if (!cop) termina("errore allocazione array coprotagonisti");
        for (int i = 0; i < numcop; i++) {
            if ((token = strtok_r(NULL, "\t", &saveptr)) == NULL) break;
            cop[i] = atoi(token);
        }

        // aggiorna struttura attori
        attore chiave = {.codice = codice_attore};
        // ricerca per codice l'indice dell'attore nell'array attori  
        attore *dest = bsearch(&chiave, sb->attori, sb->num_attori, sizeof(attore), &confronta_attori);
        if (dest) {
            dest->numcop = numcop;
            dest->cop = cop;
        } else {
            free(cop);
            termina("Codice attore non trovato nell'array");
        }

        free(line);
    }
    fprintf(stderr, "fine consumer\n");
    return NULL;
}


void leggi_grafo(const char* file_grafo, shared_buffer_t *sb, int num_consumatori) {
    FILE *f = fopen(file_grafo, "r");
    if (!f)
        termina("errore apertura file grafo");

    char *line = NULL;
    size_t len = 0;
    ssize_t e;

    while ((e = getline(&line, &len, f)) != -1) {
        char *copy = strdup(line);
        if (!copy) termina("errore strdup");

        sem_wait(&sb->emptySlots);
        pthread_mutex_lock(&sb->mutex);
        sb->buffer_linee[sb->in] = copy;
        sb->in = (sb->in + 1) % sb->capacity;
        pthread_mutex_unlock(&sb->mutex);
        sem_post(&sb->fullSlots);
    }

    // setta bool per i consumer che ha terminato di leggere il file
    atomic_store(&sb->finito, true);

    // sblocca i consumer in attesa per evitare deadlock
    for (int i = 0; i < num_consumatori; i++)
        sem_post(&sb->fullSlots);

    fprintf(stderr, "fine leggi_grafo\n");
    fclose(f);
    free(line);
}

int main(int argc, char *argv[]) {
    if (argc != 4)
        termina("Uso: cammini.out filenomi filegrafo numconsumatori\n");

    const char *file_nomi = argv[1];
    const char *file_grafo = argv[2];
    int num_consumatori = atoi(argv[3]);

    if (num_consumatori <= 0)
        termina("numero consumatori non valido, inserire un intero positivo");

    pthread_t tid;
    sigset_t set;
    shared_buffer_t sbuff = {
        .capacity = BUFFER_CAPACITY,
        .buffer_linee = malloc(sizeof(char *) * BUFFER_CAPACITY),
        .count = 0,
        .in = 0,
        .out = 0
    };
    
    if (!sbuff.buffer_linee)
        termina("errore malloc per buffer linee");
    // setta in 'pending' (non gestito) SIGINT in tutti i thread finche l'handler non lo gestisce manualmente tramite sigwait
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    pthread_sigmask(SIG_BLOCK, &set, NULL);


    atomic_bool main_terminate_flag = ATOMIC_VAR_INIT(false);
    atomic_bool main_pipe_phase = ATOMIC_VAR_INIT(false);
    signal_thread_args_t sig_args = {
        .pipe_fd = -1,
        .sigset = &set,
        .terminate_program = &main_terminate_flag,
        .pipe_phase = &main_pipe_phase
    };
    // Creiamo il thread gestore
    if (pthread_create(&tid, NULL, signal_handler_thread, (void *)&sig_args) != 0)
        termina("errore pthread_create");

    // Lettura file nomi.txt e creazione array attori
    attore *attori = NULL;
    int num_attori = carica_attori(file_nomi, &attori);
    if (num_attori < 0)
        termina("Errore durante la lettura di nomi.txt\n");

    // Lettura file grafo.txt con schema producer consumer
    sbuff.attori = attori;
    sbuff.num_attori = num_attori;
    atomic_init(&sbuff.finito, false);
    pthread_t th[num_consumatori];
    pthread_mutex_init(&sbuff.mutex, NULL);
    sem_init(&sbuff.fullSlots, 0, 0);
    sem_init(&sbuff.emptySlots, 0, sbuff.capacity);

    for (int i = 0; i < num_consumatori; i++) {
        if (pthread_create(&th[i], NULL, &consumer, (void *)&sbuff) != 0)
            perror("failed to create thread");
    }

    leggi_grafo(file_grafo, &sbuff, num_consumatori);

    for (int i = 0; i < num_consumatori; i++) {
        if (pthread_join(th[i], NULL) != 0) {
            termina("failed to join threads");
        }
    }

    // CREAZIONE E APERTURA PIPE
    fprintf(stderr, "apertura pipe\n");
    if (mkfifo("cammini.pipe", 0666) == -1) {
        if (errno != EEXIST)
        termina("coulnd't create pipe");
    }
    
    sig_args.pipe_fd = open("cammini.pipe", O_RDONLY);
    if (sig_args.pipe_fd == -1)
    termina("couldn't open pipe for reading");
    
    atomic_store(&main_pipe_phase, true);
    while (1) {
        if(atomic_load(&main_terminate_flag)) {
            fprintf(stderr, "Terminazione richiesta dal signal handler\n");
            break;
        }
        int32_t a, b;
        // n = numero byte letti dalla pipe
        ssize_t n = read(sig_args.pipe_fd, &a, sizeof(int32_t));
        if (n == 0) break;
        if (n != sizeof(int32_t)) continue;

        n = read(sig_args.pipe_fd, &b, sizeof(int32_t));
        if (n == 0) break;
        if (n != sizeof(int32_t)) continue;

        bfs_args_t *args = malloc(sizeof(bfs_args_t));
        if (!args) {
            fprintf(stderr, "errore malloc bfs_args_t\n");
            continue;
        }
        args->attori = attori;
        args->tota = num_attori;
        args->a = a;
        args->b = b;

        pthread_t bfs_tid;
        if (pthread_create(&bfs_tid, NULL, &bfs_thread, args) != 0) {
            fprintf(stderr, "errore creazione thread per cammino %d -> %d", a, b);
            free(args);
            continue;
        }
        pthread_detach(bfs_tid);
    }
    fprintf(stderr, "uscita pipe\n");
    // fine main thread
    sleep(20);
    // invia cancel request al gestore segnali
    pthread_cancel(tid);
    if (pthread_join(tid, NULL) != 0)
        termina("failed join signal handler");
    // free memory
    cleanup(&sbuff, attori, num_attori, sig_args.pipe_fd);
    return 0;
}

// SEZIONE RICERCA CAMMINO MINIMO THREAD

// Funzioni shuffle/unshuffle deterministiche per ABR bilanciato
// Dato che la lista di adiacenza di un nodo e' ordinata, l'inserimento nell'ABR creerebbe un albero degenerato, per questo viene utilizzato lo shuffle deterministico per ridurre l'altezza dell'albero e velocizzare le operazioni 
// NB : l'abr non rappresenta i cammini minimi trovati ma e' solo una struttura dati per memorizzare i nodi visitati, piu efficiente rispetto ad un semplice array

int shuffle(int n) {
    return ((((n & 0x3F) << 26) | ((n >> 6) & 0x3FFFFFF)) ^ 0x55555555);
}

int unshuffle(int n) {
    return ((((n >> 26) & 0x3F) | ((n & 0x3FFFFFF) << 6)) ^ 0x55555555);
}

// Struttura nodo ABR
typedef struct node {
    int key;
    struct node *left, *right;
} node;

void destroy_node(node *root) {
    if (root == NULL) return;
    destroy_node(root->left);
    destroy_node(root->right);
    free(root);
}

// Inserimento in ABR
node *insert_node(node *root, int key) {
    // crea nodo radice
    if (!root) {
        node *n = malloc(sizeof(node));
        if (!n) pthread_exit(NULL);
        n->key = key;
        n->left = n->right = NULL;
        return n;
    }
    if (key < root->key)
        root->left = insert_node(root->left, key);
    else if (key > root->key)
        root->right = insert_node(root->right, key);
    return root;
}

// Ricerca in ABR
bool search_node(node *root, int key) {
    if (!root) return false;
    if (key == root->key) return true;
    return (key < root->key ? search_node(root->left, key) : search_node(root->right, key));
}

// Nodo per coda BFS: memorizza INDICI nell'array attori
typedef struct queue_node {
    int attore_idx; // Indice dell'attore nell'array attori
    int from_idx;   // Indice del padre nell'array attori
    struct queue_node *next;
} queue_node;

// Coda FIFO per BFS
typedef struct {
    queue_node *front, *rear;
} queue;

queue *create_queue() {
    queue *q = malloc(sizeof(queue));
    if (!q) pthread_exit(NULL);
    q->front = q->rear = NULL;
    return q;
}

void enqueue(queue *q, int attore_idx, int from_idx) {
    queue_node *n = malloc(sizeof(queue_node));
    if (!n) pthread_exit(NULL);
    n->attore_idx = attore_idx;
    n->from_idx = from_idx;
    n->next = NULL;
    if (!q->rear) q->front = q->rear = n;
    else {
        q->rear->next = n;
        q->rear = n;
    }
}

bool dequeue(queue *q, int *attore_idx, int *from_idx) {
    if (!q->front) return false;
    queue_node *tmp = q->front;
    *attore_idx = tmp->attore_idx;
    *from_idx = tmp->from_idx;
    q->front = q->front->next;
    if (!q->front) q->rear = NULL;
    free(tmp);
    return true;
}

void destroy_queue(queue *q) {
    if (!q) return;
    int x, y;
    while (dequeue(q, &x, &y));
    free(q);
}
// BFS Thread
void *bfs_thread(void *args) {
    fprintf(stderr, "creato thread BFS\n");
    clock_t start = times(NULL);

    bfs_args_t *data = (bfs_args_t *)args;
    attore *attori = data->attori;
    int tota = data->tota;
    int a_code = data->a; 
    int b_code = data->b; 

    // Trova gli INDICI degli attori di partenza e destinazione
    attore *src_ptr = bsearch(&(attore){.codice = a_code}, attori, tota, sizeof(attore), confronta_attori);
    attore *dst_ptr = bsearch(&(attore){.codice = b_code}, attori, tota, sizeof(attore), confronta_attori);

    char nomefile[64];
    snprintf(nomefile, sizeof(nomefile), "%d.%d", a_code, b_code);
    FILE *out = fopen(nomefile, "w");
    if (!out) {
        free(data);
        pthread_exit(NULL);
    }

    if (!src_ptr || !dst_ptr) {
        if (!src_ptr) fprintf(out, "codice %d non valido\n", a_code);
        if (!dst_ptr) fprintf(out, "codice %d non valido\n", b_code);
        fclose(out);
        double elapsed_time = (double)(times(NULL) - start) / sysconf(_SC_CLK_TCK);
        fprintf(stderr, "%d.%d: Codice non valido. Tempo di elaborazione %.2f secondi\n", a_code, b_code, elapsed_time);
        free(data);
        pthread_exit(NULL);
    }
    
    // calcola l'indice
    int src_idx = (src_ptr - attori); 
    int dst_idx = (dst_ptr - attori); 

    node *explored = NULL; // ABR per i codici degli attori (shuffled) già visitati
    // coda per indici attori
    queue *q = create_queue();
    
    // array per memorizzare l'indice del padre per ogni indice di attore
    int *parent_indices = calloc(tota, sizeof(int));
    if (!parent_indices) {
        fprintf(stderr, "Errore calloc per parent_indices.\n");
        fclose(out);
        destroy_queue(q);
        destroy_node(explored);
        free(data);
        pthread_exit(NULL);
    }
    // inizializza arr padre a -1
    for (int i = 0; i < tota; i++) parent_indices[i] = -1; 

    enqueue(q, src_idx, -1);
    explored = insert_node(explored, shuffle(a_code));

    bool found = false;
    while (!found && q->front) {
        int curr_idx, father_idx;
        dequeue(q, &curr_idx, &father_idx);
        parent_indices[curr_idx] = father_idx;

        if (attori[curr_idx].codice == b_code) {
            found = true;
            break;
        }

        attore *current_attore = &attori[curr_idx]; // Accesso diretto all'attore tramite indice
        // scansiona la lista di adiacenza e aggiunge nell'abr i nodi non explored
        for (int i = 0; i < current_attore->numcop; i++) {
            int next_code = current_attore->cop[i];
            if (!search_node(explored, shuffle(next_code))) {
                // Trova l'indice del coprotagonista
                attore *next_attore_ptr = bsearch(&(attore){.codice = next_code}, attori, tota, sizeof(attore), confronta_attori);
                if (next_attore_ptr) {
                    int next_idx = (next_attore_ptr - attori); // calcola l'indice
                    enqueue(q, next_idx, curr_idx); // enqueue con l'indice del vicino e l'indice del padre
                    explored = insert_node(explored, shuffle(next_code)); // registra il codice del vicino
                    if (next_code == b_code) { // Ottimizzazione: se è la destinazione, si può uscire
                        parent_indices[next_idx] = curr_idx;
                        found = true;
                        break;
                    }
                }
            }
        }
    }

    if (!found) {
        fprintf(out, "non esistono cammini da %d a %d\n", a_code, b_code);
        double elapsed_time = (double)(times(NULL) - start) / sysconf(_SC_CLK_TCK);
        printf("%d.%d: Nessun cammino. Tempo di elaborazione %.2f secondi\n", a_code, b_code, elapsed_time);
    } else {
        // backtracking del cammino e stampa
        int path_indices[tota]; // array temporaneo per gli indici del cammino
        int len = 0, curr_path_idx = dst_idx; // inizia dall'indice di destinazione
        while (curr_path_idx != -1) {
            path_indices[len++] = curr_path_idx;
            curr_path_idx = parent_indices[curr_path_idx];
        }
        // stampa il cammino al contrario
        for (int i = len - 1; i >= 0; i--) { 
            attore *act = &attori[path_indices[i]];
            fprintf(out, "%d\t%s\t%d\n", act->codice, act->nome, act->anno);
        }
        double elapsed_time = (double)(times(NULL) - start) / sysconf(_SC_CLK_TCK);
        printf("%d.%d: Lunghezza minima %d. Tempo di elaborazione %.2f secondi\n", a_code, b_code, len -1, elapsed_time);
    }

    free(data);
    fclose(out);
    destroy_queue(q);
    free(parent_indices);
    destroy_node(explored);
    pthread_exit(NULL);
}

void termina(const char *messaggio) {
    if (errno == 0)
        fprintf(stderr, "%s\n", messaggio);
    else
        perror(messaggio);
    exit(EXIT_FAILURE);
}

void cleanup(shared_buffer_t *sb, attore *attori, int num_attori, int pipe_fd) {
    // chiudo pipe se aperta
    if (pipe_fd >= 0)
        close(pipe_fd);

    // distruggo semafori e mutex
    sem_destroy(&sb->emptySlots);
    sem_destroy(&sb->fullSlots);
    pthread_mutex_destroy(&sb->mutex);

    free(sb->buffer_linee);
    free_array_attori(attori, num_attori);
    unlink("cammini.pipe");
}