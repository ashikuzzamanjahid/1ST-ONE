// task1.c - Shared Memory + Pipe (home & opr processes) - FIXED VERSION
// Compile: gcc -Wall -Wextra -O2 -std=c11 task1.c -o task1
// Run: ./task1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <errno.h>

struct shared {
    char sel[100]; // user's selection
    int b;         // current balance
};

static void die(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int main(void) {
    // make stdout unbuffered so prompts appear immediately
    setvbuf(stdout, NULL, _IONBF, 0);

    // Create shared memory
    int shmid = shmget(IPC_PRIVATE, sizeof(struct shared), IPC_CREAT | 0666);
    if (shmid == -1) die("shmget");

    // Attach shared memory in parent
    struct shared *sh = (struct shared *)shmat(shmid, NULL, 0);
    if (sh == (void *)-1) {
        int e = errno;
        // Cleanup shm on attach failure
        shmctl(shmid, IPC_RMID, NULL);
        errno = e;
        die("shmat (parent)");
    }

    // Create pipe
    int fds[2];
    if (pipe(fds) == -1) {
        int e = errno;
        shmdt(sh);
        shmctl(shmid, IPC_RMID, NULL);
        errno = e;
        die("pipe");
    }

    printf("Execution Command in Terminal: ./task1\n");
    printf("Provide Your Input From Given Options:\n");
    printf("1. Type a to Add Money\n");
    printf("2. Type w to Withdraw Money\n");
    printf("3. Type c to Check Balance\n");

    char input[128];
    if (!fgets(input, sizeof(input), stdin)) {
        fprintf(stderr, "Failed to read input.\n");
        // Cleanup
        close(fds[0]); close(fds[1]);
        shmdt(sh);
        shmctl(shmid, IPC_RMID, NULL);
        return EXIT_FAILURE;
    }

    // Trim whitespace/newline
    size_t len = strcspn(input, "\r\n");
    input[len] = '\0';

    // Initialize shared state
    memset(sh->sel, 0, sizeof(sh->sel));
    strncpy(sh->sel, input, sizeof(sh->sel) - 1);
    sh->b = 1000;

    printf("Your selection: %s\n", sh->sel);

    pid_t pid = fork();
    if (pid < 0) {
        int e = errno;
        close(fds[0]); close(fds[1]);
        shmdt(sh);
        shmctl(shmid, IPC_RMID, NULL);
        errno = e;
        die("fork");
    }

    if (pid == 0) {
        // Child process: opr
        // Close read end of pipe, we only write
        close(fds[0]);

        // Attach shared memory
        struct shared *child_sh = (struct shared *)shmat(shmid, NULL, 0);
        if (child_sh == (void *)-1) {
            // Even on failure, try to notify parent
            const char *msg = "Thank you for using\n";
            write(fds[1], msg, strlen(msg));
            close(fds[1]);
            _exit(EXIT_FAILURE);
        }

        // Act on selection
        if (strcmp(child_sh->sel, "a") == 0) {
            printf("Enter amount to be added:\n");
            char buf[128];
            if (!fgets(buf, sizeof(buf), stdin)) {
                printf("Adding failed, Invalid amount\n");
            } else {
                char *endp = NULL;
                long amt = strtol(buf, &endp, 10);
                if (endp == buf || amt <= 0) {
                    printf("Adding failed, Invalid amount\n");
                } else {
                    child_sh->b += (int)amt;
                    printf("Balance added successfully\n");
                    printf("Updated balance after addition:\n");
                    printf("%d\n", child_sh->b);
                }
            }
        } else if (strcmp(child_sh->sel, "w") == 0) {
            printf("Enter amount to be withdrawn:\n");
            char buf[128];
            if (!fgets(buf, sizeof(buf), stdin)) {
                printf("Withdrawal failed, Invalid amount\n");
            } else {
                char *endp = NULL;
                long amt = strtol(buf, &endp, 10);
                // FIXED: Amount must be > 0 AND < current balance (not >=)
                // This means you cannot withdraw the entire balance
                if (endp == buf || amt <= 0 || amt >= child_sh->b) {
                    printf("Withdrawal failed, Invalid amount\n");
                } else {
                    child_sh->b -= (int)amt;
                    printf("Balance withdrawn successfully\n");
                    printf("Updated balance after withdrawal:\n");
                    printf("%d\n", child_sh->b);
                }
            }
        } else if (strcmp(child_sh->sel, "c") == 0) {
            printf("Your current balance is:\n");
            printf("%d\n", child_sh->b);
        } else {
            printf("Invalid selection\n");
        }

        // Write thank you to pipe
        const char *msg = "Thank you for using\n";
        (void)write(fds[1], msg, strlen(msg));

        // Cleanup
        shmdt(child_sh);
        close(fds[1]);
        _exit(0);
    } else {
        // Parent process: home
        // Close write end; we only read
        close(fds[1]);

        // Wait for child to finish before reading (as per spec)
        int status = 0;
        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid");
        }

        // Read from pipe
        char buf[256];
        ssize_t n = read(fds[0], buf, sizeof(buf) - 1);
        if (n >= 0) {
            buf[n] = '\0';
            printf("%s", buf);  // FIXED: Removed extra newline
        } else {
            perror("read (pipe)");
        }

        // Cleanup
        close(fds[0]);
        shmdt(sh);
        shmctl(shmid, IPC_RMID, NULL);

        return WIFEXITED(status) ? WEXITSTATUS(status) : EXIT_FAILURE;
    }
}