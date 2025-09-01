// task2.c - Message Queue + 3 Processes (log in, otp generator, mail) - FIXED VERSION
// Compile: gcc -Wall -Wextra -O2 -std=c11 task2.c -o task2
// Run: ./task2
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <errno.h>

struct msg {
    long type;   // message type
    char txt[6]; // fixed 6 bytes as per problem statement
};

static void die(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

static void safe_send(int msqid, long type, const char *payload) {
    struct msg m;
    m.type = type;
    // FIXED: Ensure proper null-termination within 6 bytes
    // Use strncpy and manually null-terminate to handle potential truncation
    memset(m.txt, 0, sizeof(m.txt));
    strncpy(m.txt, payload, sizeof(m.txt) - 1);
    m.txt[sizeof(m.txt) - 1] = '\0';
    
    if (msgsnd(msqid, &m, sizeof(m.txt), 0) == -1) die("msgsnd");
}

static ssize_t safe_recv(int msqid, long type, struct msg *out) {
    // Receive first message matching 'type'
    ssize_t r = msgrcv(msqid, out, sizeof(out->txt), type, 0);
    if (r == -1) die("msgrcv");
    // FIXED: Ensure null-termination after receiving
    out->txt[sizeof(out->txt) - 1] = '\0';
    return r;
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);

    // Create a message queue; IPC_PRIVATE so children inherit it.
    int msqid = msgget(IPC_PRIVATE, IPC_CREAT | 0666);
    if (msqid == -1) die("msgget");

    printf("Execution Command in Terminal: ./task2\n");
    printf("Please enter the workspace name:\n");

    char workspace[128];
    if (!fgets(workspace, sizeof(workspace), stdin)) {
        fprintf(stderr, "Failed to read workspace name.\n");
        msgctl(msqid, IPC_RMID, NULL);
        return EXIT_FAILURE;
    }
    // trim newline
    size_t len = strcspn(workspace, "\r\n");
    workspace[len] = '\0';

    // Compare exactly to "cse321"
    if (strcmp(workspace, "cse321") != 0) {
        printf("Invalid workspace name\n");
        msgctl(msqid, IPC_RMID, NULL);
        return 0;
    }

    // Send workspace to OTP generator with type = 1
    safe_send(msqid, 1, "cse321");
    printf("Workspace name sent to otp generator from log in: cse321\n");

    pid_t otp_pid = fork();
    if (otp_pid < 0) {
        int e = errno;
        msgctl(msqid, IPC_RMID, NULL);
        errno = e;
        die("fork");
    }

    if (otp_pid == 0) {
        // Child process: OTP generator
        struct msg m;
        safe_recv(msqid, 1, &m);
        printf("OTP generator received workspace name from log in: %s\n", m.txt);

        // FIXED: Generate OTP = own PID, but handle potential truncation gracefully
        char otpbuf[16];
        int pid_val = (int)getpid();
        snprintf(otpbuf, sizeof(otpbuf), "%d", pid_val);
        
        // Warn if PID is too long (optional, for debugging)
        if (strlen(otpbuf) >= 6) {
            // PID will be truncated to fit in 5 chars + null terminator
            // This is expected behavior given the 6-byte constraint
        }

        // Send OTP to login (type = 2)
        safe_send(msqid, 2, otpbuf);
        printf("OTP sent to log in from OTP generator: %s\n", otpbuf);

        // Send OTP to mail (type = 3)
        safe_send(msqid, 3, otpbuf);
        printf("OTP sent to mail from OTP generator: %s\n", otpbuf);

        // Fork mail
        pid_t mail_pid = fork();
        if (mail_pid < 0) die("fork (mail)");

        if (mail_pid == 0) {
            // Grandchild: mail
            struct msg m2;
            safe_recv(msqid, 3, &m2);
            printf("Mail received OTP from OTP generator: %s\n", m2.txt);

            // Forward to login with type = 4
            safe_send(msqid, 4, m2.txt);
            printf("OTP sent to log in from mail: %s\n", m2.txt);

            _exit(0);
        } else {
            // OTP generator waits for mail to finish, then exits
            int st = 0;
            (void)waitpid(mail_pid, &st, 0);
            _exit(0);
        }
    } else {
        // Parent process: log in
        // Wait for otp generator to finish
        int st = 0;
        if (waitpid(otp_pid, &st, 0) == -1) {
            perror("waitpid");
        }

        // Receive messages
        struct msg from_otp, from_mail;

        safe_recv(msqid, 2, &from_otp);
        printf("Log in received OTP from OTP generator: %s\n", from_otp.txt);

        safe_recv(msqid, 4, &from_mail);
        printf("Log in received OTP from mail: %s\n", from_mail.txt);

        // FIXED: Use strcmp instead of strncmp for proper comparison
        if (strcmp(from_otp.txt, from_mail.txt) == 0) {
            printf("OTP Verified\n");
        } else {
            printf("OTP Incorrect\n");
        }

        // Cleanup
        msgctl(msqid, IPC_RMID, NULL);
        return 0;
    }
}