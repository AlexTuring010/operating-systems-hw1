#include <stdio.h>
#include <semaphore.h>
#include <stdlib.h>
#include <string.h>

int max_semaphores;
sem_t* sem_array;
int process_to_sem_map;
int semaphores_created = 0;
int active_processes = 0;

// Μέσω αυτού επιστρέφω την parsed γραμμή
typedef struct{
    int step;
    int process;
    char command;
} PL;

parse_instruction parse_line(char* line){
    char *word1, *word2, *word3;
    PL parsed_line;
    word1 = strtok(line, " ");
    word2 = strtok(NULL, " ");
    parsed_line.step = atoi(word1);
    if(strcmp(word2, "EXIT") == 0){
        memmove(word2, word2 + 1, strlen(str)); // Για να φύγει ο χαρακτήρας C και να μείνει μόνο ο αριθμός
        parsed_line.process = atoi(word2) - 1;  // -1 γιατί εγώ μετράω από 0 αλλά το config από 1
        parsed_line.command = 'E';
    } else{
        word3 = strtok(NULL, " ");
        parsed_line.command = word3[0];
    }
    return parsed_line;
}

int main(int argc, char* argv[]){
    if(argc < 3){
        printf("Usage: ./my_program <path_to_config> <path_to_text> <max_num_of_semaphores>\n");
        return 1;
    }
    char* path_to_config = argv[0];
    char* path_to_text = argv[1];
    max_semaphores = atoi(argv[2]);
    FILE* config_file = fopen(path_to_config, "r");
    FILE* text_file = fopen(path_to_text, "r");
    if(config_file == NULL || text_file == NULL){
        printf("Failed to open the files.\n");
        return 1;
    }
    sem_array = malloc(sizeof(sem_t) * max_semaphores);
    if(sem_array = NULL){
        printf("Failed to allocate memory.\n");
        return 1;
    }

    int step = 1; // Το βήμα στο οποίο βρίσκεται η εκτέλεση αυτή την στιγμή
    char line[256]; // Θεωρώ πως οι γραμμές θα έχουν max 257 χαρακτήρες
    while (fgets(line, sizeof(line), config_file)) {
        PL parsed_line = parse_line(line);
        if(parsed_line.command == 'E'){

        } else if(parsed_line.command == 'S'){

        } else if(parsed_line.command == 'T'){

        } else{
            printf("Read unknown command. \n");
        }
        step++;
    }


    fclose(config_file);
    fclose(text_file);
    return 0;
}