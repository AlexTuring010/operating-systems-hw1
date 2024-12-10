#include <stdio.h>   // For standard input/output functions like printf()
#include <stdlib.h>  // For standard library functions like exit()
#include <unistd.h>  // For system calls like fork(), sleep(), etc.
#include <semaphore.h>  // For POSIX semaphore functions (sem_open, sem_wait, sem_post, etc.)
#include <fcntl.h>   // For file control options like O_CREAT (used with sem_open)
#include <sys/stat.h>  // For defining file permission modes like 0644 (used with sem_open)

#define SEM_NAME "/example_semaphore"  // Name for the named semaphore

void child_process(sem_t *sem) {
    // Child does some work
    printf("Child process: Starting work...\n");
    sleep(2);  // Simulating work with a 2-second sleep
    printf("Child process: Work completed.\n");

    // Signal to parent that work is done
    sem_post(sem);  // This increments the semaphore
}

int main() {
    sem_t *sem;
    
    // Create and initialize the named semaphore
    sem = sem_open(SEM_NAME, O_CREAT | O_EXCL, 0644, 0);  // Initialize semaphore to 0 (blocked)
    if (sem == SEM_FAILED) {
        perror("sem_open");
        exit(1);
    }

    pid_t pid = fork();
    
    if (pid == -1) {
        perror("fork");
        exit(1);
    } else if (pid == 0) {  // Child process
        // Child process work
        child_process(sem);
        exit(0);  // Exit child process after completing work
    } else {  // Parent process
        // Parent process waits for the child to finish
        printf("Parent process: Waiting for child to complete...\n");
        sem_wait(sem);  // Parent waits until semaphore is posted by the child
        
        printf("Parent process: Child completed its work. Continuing...\n");

        // Cleanup: Close and unlink the semaphore
        sem_close(sem);
        sem_unlink(SEM_NAME);  // Unlink the named semaphore
    }

    return 0;
}

// sem_open - Open or create a named semaphore
// Parameters:
// 1. name: The name of the semaphore (must start with '/'). It is used to identify the semaphore.
//    Example: "/example_semaphore"
// 2. oflag: Flags to control the behavior. Common flags include:
//    - O_CREAT: Create the semaphore if it does not exist.
//    - O_EXCL: Fail if the semaphore already exists.
//    - O_RDWR: Open for reading and writing (not usually needed for semaphores).
// 3. mode: The permissions for the semaphore. It is only used when creating the semaphore (with O_CREAT flag).
//    Example: 0644 (read and write for the owner, read-only for others).
// 4. value: The initial value of the semaphore. 
//    Example: 0 (locked), 1 (unlocked), or higher for multiple processes allowed.
// Returns:
// - A pointer to the semaphore (sem_t*) if successful.
// - SEM_FAILED if an error occurs.
sem_t *sem_open(const char *name, int oflag, mode_t mode, unsigned int value);


// sem_wait - Decrement the semaphore (locks it), blocking if the value is 0
// Parameters:
// 1. sem: A pointer to the semaphore to decrement (lock).
// Returns:
// - 0 on success.
// - -1 on failure (check errno for error details).
int sem_wait(sem_t *sem);


// sem_post - Increment the semaphore (unlocks it), allowing other processes to continue
// Parameters:
// 1. sem: A pointer to the semaphore to increment (unlock).
// Returns:
// - 0 on success.
// - -1 on failure (check errno for error details).
int sem_post(sem_t *sem);


// sem_close - Close the semaphore (after usage)
// Parameters:
// 1. sem: A pointer to the semaphore to be closed.
// Returns:
// - 0 on success.
// - -1 on failure (check errno for error details).
int sem_close(sem_t *sem);


// sem_unlink - Remove a named semaphore from the system
// Parameters:
// 1. name: The name of the semaphore to remove.
// Returns:
// - 0 on success.
// - -1 on failure (check errno for error details).
int sem_unlink(const char *name);

