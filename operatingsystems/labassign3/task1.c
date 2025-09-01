// task1.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <errno.h>

struct shared {
    char sel[100];
    int b;
};

int main() {
    int shmid = shmget(IPC_PRIVATE, sizeof(struct shared), IPC_CREAT | 0666);
    struct shared *sh = (struct shared *)shmat(shmid, NULL, 0);
    
    int pipefd[2];
    pipe(pipefd);

    printf("Execution Command in Terminal: ./task1\n");
    printf("Provide Your Input From Given Options:\n");
    printf("1. Type a to Add Money\n");
    printf("2. Type w to Withdraw Money\n");
    printf("3. Type c to Check Balance\n");

    char input[128];
    fgets(input, sizeof(input), stdin);
    
    // remove newline
    input[strlen(input)-1] = '\0';
    
    strcpy(sh->sel, input);
    sh->b = 1000;

    printf("Your selection: %s\n", sh->sel);

    pid_t pid = fork();
    
    if (pid == 0) {
        // child - opr process
        close(pipefd[0]);
        
        struct shared *child_sh = (struct shared *)shmat(shmid, NULL, 0);
        
        if (strcmp(child_sh->sel, "a") == 0) {
            printf("Enter amount to be added:\n");
            char buf[128];
            fgets(buf, sizeof(buf), stdin);
            int amt = atoi(buf);
            if (amt > 0) {
                child_sh->b += amt;
                printf("Balance added successfully\n");
                printf("Updated balance after addition:\n");
                printf("%d\n", child_sh->b);
            } else {
                printf("Adding failed, Invalid amount\n");
            }
        } 
        else if (strcmp(child_sh->sel, "w") == 0) {
            printf("Enter amount to be withdrawn:\n");
            char buf[128];
            fgets(buf, sizeof(buf), stdin);
            int amt = atoi(buf);
            if (amt > 0 && amt < child_sh->b) {
                child_sh->b -= amt;
                printf("Balance withdrawn successfully\n");
                printf("Updated balance after withdrawal:\n");
                printf("%d\n", child_sh->b);
            } else {
                printf("Withdrawal failed, Invalid amount\n");
            }
        } 
        else if (strcmp(child_sh->sel, "c") == 0) {
            printf("Your current balance is:\n");
            printf("%d\n", child_sh->b);
        } 
        else {
            printf("Invalid selection\n");
        }

        char *msg = "Thank you for using\n";
        write(pipefd[1], msg, strlen(msg));
        
        close(pipefd[1]);
        exit(0);
    } 
    else {
        // parent - home process
        close(pipefd[1]);
        
        wait(NULL);
        
        char buf[256];
        int n = read(pipefd[0], buf, 255);
        buf[n] = '\0';
        printf("%s", buf);

        close(pipefd[0]);
        shmdt(sh);
        shmctl(shmid, IPC_RMID, NULL);
    }

    return 0;
}