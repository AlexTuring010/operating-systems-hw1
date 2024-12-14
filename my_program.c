// Made by Alex Gkiafis sdi2200284

#include <stdio.h>               // Περιλαμβάνει λειτουργίες εισόδου/εξόδου, όπως printf και scanf
#include <stdlib.h>              // Περιλαμβάνει συναρτήσεις γενικής χρήσης, όπως malloc, free, exit, atoi
#include <string.h>              // Περιλαμβάνει συναρτήσεις χειρισμού συμβολοσειρών, όπως strcpy, strcat, strlen
#include <time.h>                // Περιλαμβάνει συναρτήσεις χρόνου
#include <unistd.h>              // Περιλαμβάνει λειτουργίες του συστήματος UNIX, όπως fork, sleep, getpid
#include <sys/wait.h>            // Για τη χρήση των waitpid() και μακροεντολών όπως WEXITSTATUS
#include <sys/types.h>           // Για τύπους δεδομένων όπως size_t, pid_t, κλπ.
#include <sys/mman.h>            // Για τις συναρτήσεις mmap(), munmap(), PROT_READ, PROT_WRITE, MAP_SHARED
#include <semaphore.h>           // Για τη διαχείριση POSIX σεμαφόρων, όπως sem_init, sem_wait, sem_post

#define NUM_OF_LINES 22315       // Για να μην χρειαστεί να φτιάξω συνάρτηση που το κάνει, απλά το βάζω έτοιμο εδώ
#define LINE_MAX_SIZE 1024       // Θεωρώ πως δεν θα υπάρξει γραμμή με περισσότερους από τόσους χαρακτήρες

// Η κοινή μνήμη θα αποθηκευτεί με αυτή την δομή
typedef struct {
    char data[LINE_MAX_SIZE];    // Τα δεδομένα τα οποία στέλονται
    int command;                 // Εντολή (1 για διάβασμα, 0 για τερματισμό)
    int step;                    // Δείχνει σε ποιο βήμα βρησκόμαστε
    sem_t* read_semaphores;      // Σεμοφόροι που μπλοκάρουν το διάβασμα
    sem_t write_semaphore;       // Σεμαφόρος που μπλοκάρει το γράψιμο (χρειάζομαι μόνο έναν)
} SharedMemory;

SharedMemory* shared_memory;     // Η διεύθυνση εδώ θα δείχνει σε shared memory!

// Θα χρειαστεί να κάνω λίγο tracking, στην ουσία, με ενδιαφέρουν τα εξής:

// 1) Είναι μια διαδικασία (process) ενεργή, και αν ναι, ποιους σεμοφόρους χρησιμοποιεί?
// 2) Σε περίπτωση δημιουργίας νέας διαδικασίες, υπάρχουν διαθέσιμοι σεμαφόροι και ποιοί?

// Το παραπάνω tracking δεν θα χρειάζεται να ενημερώνεται για τις διεργασίες παιδιά
// Για αυτό και δεν θα χρειαστεί να μπει στο shared memory

typedef struct{
    pid_t pid;                   // Το πραγματικό pid της διεργασία (το θέλω για το waitpid)
    int is_active;               // Θα είναι 0 αν όχι και 1 αν ναι
    int sem_index;               // Τα indexes των semaphores για την συγκεκριμένη διαδικασία
} process_info;

int active_processes = 0;        // Πόσες ενεργές διεργασίες υπάρχουν (καμία ακόμα)
int max_process_id = -1;         // Μπορεί να χρησιμοποιηθούν ids μεγαλύτερες του max_semaphores και πρέπει να το διαχειριστώ αυτό

// Αυτό το χρησιμοποιώ για να "μαζέψω" τις ενεργές διεργασίες και να επιλέξω μία στην τύχη να της στείλω μήνυμα
int* available_processes;

typedef struct{
    int is_available;            // Θα είναι 0 αν όχι και 1 αν ναι
} semaphore_info;

int max_semaphores;              // Μέγιστο αριθμός σεμαφόρων που μπορούν να υπάρξουν
int num_of_semaphores = 0;       // Πόσοι σεμαφόροι υπάρχουν αυτή την στιγμή (κανένας αρχικά)

process_info* process_tracking;  // Σε αυτό το array θα κάνω το tracking για τις processes, χρειάζεται realloc όταν βρίσκουμε νέο max_process_id
semaphore_info* sem_tracking;    // Σε αυτό το array θα κάνω το tracking για τους σεμαφόρους

int find_available_semaphore(){
    for(int i = 0; i < num_of_semaphores; i++){
        if(sem_tracking[i].is_available){
            return i;
        }
    }
    return -1; // Δεν υπάρχει διαθέσιμος σεμαφόρος
}

char* book_lines[NUM_OF_LINES];  // Αποθηκεύει τις γραμμές του βιβλίου

// Το πως ακριβως γίνεται το tracking βγάζει περισσότερο νόημα όταν δεις τον κώδικα αργότερα

// Χρειάζομαι μια συνάρτηση να αναλάβει να κάνει το parsing της γραμμής που θα διαβάζουμε από το config αρχείο

typedef struct{
    int step;
    int process;
    char command;
} parsed_line;

parsed_line parse_line(char* line){
    char *word1, *word2, *word3;
    parsed_line pline;
    word1 = strtok(line, " ");
    word2 = strtok(NULL, " ");
    pline.step = atoi(word1);
    if(strcmp(word2, "EXIT") == 0 || strcmp(word2, "EXIT\n") == 0){
        pline.command = 'E';
    } else{
        memmove(word2, word2 + 1, strlen(word2));
        pline.process = atoi(word2);
        word3 = strtok(NULL, " ");
        pline.command = word3[0];
    }
    return pline;
}

// Την παραπάνω συνάρτηση την τρέχουν όλα τα παιδιά διεργασίες εφόσον δημιουργηθούν

void run_child_process(int process_id, int sem_id, int step){
    printf("(%d) C%d has been spawned!\n", step, process_id);
    int messages_recieved = 0;
    int start_step = step;
    while(1){
        sem_wait(&shared_memory->read_semaphores[sem_id]);
        int end_step = shared_memory->step;
        if(shared_memory->command == 0){
            int total_steps = end_step - start_step;
            printf("(%d) C%d: Terminated: It recieved %d messages and was active for %d steps\n", end_step, process_id, messages_recieved, total_steps);
            sem_post(&shared_memory->write_semaphore);
            break;
        }
        printf("(%d) C%d: Recieved Message: %s\n", end_step, process_id, shared_memory->data);
        messages_recieved += 1;
        sem_post(&shared_memory->write_semaphore);
    }   
}

// Η συνάρτηση που κάνει spawn νέες διεργασίες. Αναλαμβάνει την αντιστοίχηση διεργασίας
// με σημαφόρο. Προσπαθεί να φτιάχνει νέους σημαφόρους μόνο όταν πραγματικά χρειάζεται.

int spawn_process(int process_id, int step, FILE* config_file){
    if(process_id > max_process_id){     // Εύκολο fix σε περίπτωση που χρησιμοποιηθεί process id μεγαλύτερο
        int prev_max = max_process_id;   // του max_semaphores. Έτσι και αλλίως, δεν περιμένουμε τεράστια ids
        max_process_id = process_id;     // Και ο πίνακας προσφέρει γρήγορο search πολυπλοκότητας O(1)
        process_tracking = realloc(process_tracking, sizeof(process_info) * (max_process_id+1));
        available_processes = realloc(available_processes, sizeof(int) * (max_process_id+1));
        for(int i = prev_max+1; i <= max_process_id; i++){
            process_tracking[i].is_active = 0;
        }
    }
    if(active_processes == max_semaphores){
        printf("ERROR: Not enough semaphores to create process C%d\n", process_id);
        return 0;
    }
    if(process_tracking[process_id].is_active){
        printf("ERROR: Cannot spawn process C%d because it is already active!\n", process_id);
        return 0;
    }
    int sem_id;
    if(active_processes == num_of_semaphores){
        sem_id = num_of_semaphores;
        if(sem_init(&shared_memory->read_semaphores[sem_id], 1, 0) != 0){
            printf("ERROR: sem_init(sem_read) failed \n");
            return 0;
        }
        num_of_semaphores += 1;
    } else{
        sem_id = find_available_semaphore();
    }
    sem_tracking[sem_id].is_available = 0;
    process_tracking[process_id].is_active = 1;
    process_tracking[process_id].sem_index = sem_id;
    pid_t pid = fork();
    if(pid == 0){
        fclose(config_file);
        free(process_tracking);
        free(sem_tracking);
        free(available_processes);
        for(int i = 0; i < NUM_OF_LINES; i++){
            free(book_lines[i]);
        }
        run_child_process(process_id, sem_id, step);
        exit(0); // Τερματίζει την διεργασία παιδί
    } else if(pid < 0){
        process_tracking[process_id].is_active = 0;
        sem_tracking[sem_id].is_available = 1;
        printf("ERROR: fork failed \n");
    } else{
        active_processes += 1;
        process_tracking[process_id].pid = pid;
    }
    return 0;
}

// Συνάρτηση που κάνει terminate μια διεργασία παιδί
// Αυτό γίνεται περνώντας κατάλληλο μήνυμα στην διαδικασία μέσω της κοινής μνήμης

int terminate_process(int process_id, int step){
    if(process_tracking[process_id].is_active == 0){
        printf("ERROR: C%d cannot be terminated because it is not active\n", process_id);
        return -1;
    }
    int sem_id = process_tracking[process_id].sem_index;
    sem_wait(&shared_memory->write_semaphore);
    shared_memory->command = 0;
    shared_memory->step = step;
    sem_post(&shared_memory->read_semaphores[sem_id]);
    process_tracking[process_id].is_active = 0;
    active_processes -= 1;
    sem_tracking[process_tracking[process_id].sem_index].is_available = 1;
    return process_id;
}

// Συνάρτηση για να τερματίζει το πρόγραμμα
// Αναλαμβάνει τον τερματισμό όλων των processes

void exit_program(int step){
    for(int i = 0; i <= max_process_id; i++){
        if(process_tracking[i].is_active){
            terminate_process(i, step);
        }
    }
    pid_t pid = 1;
    while (pid > 0) { // Reap one child at a time
        pid = waitpid(-1, NULL, 0);
    }
}

// Θα χρειαστώ έναν γρήγορο τρόπο να διαλέξω τυχαία γραμμές από το αρχείο.
// Επέλεξα απλά να βάλω όλλες τις γραμμές σε έναν πίνακα στην μνήμη

void store_lines_in_memory(FILE* file){
    char buffer[LINE_MAX_SIZE];
    int line = 0;
    while(fgets(buffer, sizeof(buffer), file)){
        buffer[strcspn(buffer, "\n")] = '\0';
        book_lines[line] = malloc(strlen(buffer) + 1);
        if(book_lines[line] == NULL){
            printf("ERROR: Failed to allocate memory for a line");
            return;
        }
        strcpy(book_lines[line], buffer);
        line += 1;
        if(line == NUM_OF_LINES){
            break;
        }
    }
}

// Η συνάρτηση που στέλνει μια γραμμή στην τύχη σε ένα τυχαίο παιδί

void send_random_message(int step){
    int random_line_index = rand() % NUM_OF_LINES;
    int j = 0;
    for(int i = 0; i <= max_process_id; i++){
        if(process_tracking[i].is_active){
            available_processes[j] = i;
            j++;
        }
    }
    int random_process_index = rand() % active_processes;
    int process_id = available_processes[random_process_index];
    sem_wait(&shared_memory->write_semaphore);
    shared_memory->step = step;
    shared_memory->command = 1;
    strcpy(shared_memory->data, book_lines[random_line_index]);
    int sem_id = process_tracking[process_id].sem_index;
    sem_post(&shared_memory->read_semaphores[sem_id]);
}

// H main συνάρτηση του προγράμματος

int main(int argc, char* argv[]){
    srand(time(NULL));

    // Διαβάζω τα arguments του προγράμματος
    if(argc < 3){
        printf("Usage: ./my_program <path_to_config> <path_to_text> <max_semaphores>\n");
        return 1;
    }
    char* path_to_config = argv[1];
    char* path_to_text = argv[2];
    max_semaphores = atoi(argv[3]);
    if(max_semaphores < 1){
        printf("<max_semaphores> should be an integer greater than 0!\n");
        return 1;
    }

    // Ανοίγω τα αρχεία που θα χρειαστώ
    FILE* config_file = fopen(path_to_config, "r");
    FILE* text_file = fopen(path_to_text, "r");
    if(config_file == NULL || text_file == NULL){
        printf("Failed to open the files.\n");
        return 1;
    }
   
    // Δημιουργώ την κοινή μνήμη με επιπλέον χώρο για τους σεμαφόρους στο τέλος
    // Βάζω τον pointer της array των σεμαφόρων να δείχνει σε αυτό τον έξτρα χώρο
    size_t size = sizeof(SharedMemory) + max_semaphores * sizeof(sem_t);
    shared_memory = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shared_memory == MAP_FAILED) {
        printf("ERROR: Failed to allocate shared memory");
        return 1;
    }
    shared_memory->read_semaphores = (sem_t*)((char*)shared_memory + sizeof(SharedMemory));

    // Κάνω allocate μνήμη για τους διάφορους πίνακες που θα χρειαστώ 
    process_tracking = malloc(sizeof(process_info) * (max_semaphores+1));
    max_process_id = max_semaphores;
    sem_tracking = malloc(sizeof(semaphore_info) * max_semaphores);
    available_processes = malloc(sizeof(int) * max_semaphores);
    if(process_tracking == NULL || sem_tracking == NULL || available_processes == NULL){
        printf("Failed to allocate memory.\n");
        return 1;
    }

    // Κάποιες αρχηκοποιήσεις που πρέπει να γίνουν
    for(int i = 0; i < max_semaphores; i++){
        process_tracking[i].is_active = 0;
        sem_tracking[i].is_available = 1;
    }
    process_tracking[max_semaphores].is_active = 0;

    char line[LINE_MAX_SIZE];

    parsed_line pline;  // Το pline κρατάει την νεότερη parsed γραμμή του config που έχουμε διαβάσει ως τώρα
    pline.step = -1;    // Θέτοντας αυτό σε -1 κάνω signal να διαβαστεί νέα γραμμή στο config, γιατί έτσι

    store_lines_in_memory(text_file);
    fclose(text_file);  // Δεν θα το χρειαστώ άλλο

    if(sem_init(&shared_memory->write_semaphore, 1, 1) != 0){
        printf("ERROR: sem_init(seam_write) failed \n");
        return 1;
    }

    int step = 1;       // Βήμα αντιστοιχεί σε iteration της while επανάληψης πιο κάτω

    int process_id = -1;

    // Και είμαστε έτοιμοι να ξεκεινήσουμε το πρόγραμμα!
    while(1){
        if(process_id  >= 0){
            // Επειδή η εκφώνηση γράφει: Η σχετική πληροφορία (exit code) Θα 
            // πρέπει να απορροφηθεί από τον γονέα P στο επόμενο βήμα εκτέλεσης
            waitpid(process_tracking[process_id].pid, NULL, 0);
            process_id = -1;
        }
        if(pline.step == -1){
            if(fgets(line, sizeof(line), config_file) == NULL){
                break; // Σε περίπτωση που το config δεν έχει εντολή ΕΧΙΤ, κανονικά πρέπει να έχει όμως
            }
            pline = parse_line(line);
            if(pline.process < 1 && pline.command != 'E'){
                printf("ERROR: processes cannot have ID less than 1\n");
                pline.step = -1;
            } else if(pline.step < step){
                printf("ERROR: too late to execute the instruction, config file should list them in increasing order\n");
                pline.step = -1;
            }
        }
        if(pline.step == step){ // Εκτελεί την εντολή στο σωστό βήμα
            if(pline.command == 'S'){
                spawn_process(pline.process, step, config_file);
            } else if(pline.command == 'T'){
                process_id = terminate_process(pline.process, step);
            } else if(pline.command == 'E'){
                exit_program(step);
                break;
            }
            pline.step = -1; // Κάνει signal να διαβαστεί νέα ενέργεια στο επόμενο iteration
        }
        if(active_processes > 0){
            send_random_message(step); // Σε κάθε βήμα θα στέλνει και ένα τυχαίο μήνυμα σε ένα τυχαίο παιδί!
        } else{
            printf("(%d) there are no active processes to send messages to\n", step);
        }
        step += 1;
    }

    // Στο τέλος κάνουμε και free και τα λοιπά

    fclose(config_file);
    free(process_tracking);
    free(sem_tracking);
    free(available_processes);
    for(int i = 0; i < NUM_OF_LINES; i++){
        free(book_lines[i]);
    }

    for (int i = 0; i < num_of_semaphores; i++) {
        sem_destroy(&shared_memory->read_semaphores[i]);
    }
    sem_destroy(&shared_memory->write_semaphore);

    munmap(shared_memory, size); // Μόνο ο πατέρας κάνει unmap την shared memory
    return 0;
}

