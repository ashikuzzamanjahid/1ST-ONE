// task2.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>

struct msg {
    long type;
    char txt[6];
};

int main() {
    int msqid = msgget(IPC_PRIVATE, IPC_CREAT | 0666);

    printf("Execution Command in Terminal: ./task2\n");
    printf("Please enter the workspace name:\n");

    char workspace[100];
    fgets(workspace, sizeof(workspace), stdin);
    workspace[strlen(workspace)-1] = '\0'; // remove newline

    if (strcmp(workspace, "cse321") != 0) {
        printf("Invalid workspace name\n");
        msgctl(msqid, IPC_RMID, NULL);
        return 0;
    }

    // send workspace to otp generator
    struct msg m1;
    m1.type = 1;
    strncpy(m1.txt, "cse321", 5);
    m1.txt[5] = '\0';
    msgsnd(msqid, &m1, sizeof(m1.txt), 0);
    printf("Workspace name sent to otp generator from log in: cse321\n");

    pid_t otp_pid = fork();
    
    if (otp_pid == 0) {
        // otp generator process
        struct msg recv_msg;
        msgrcv(msqid, &recv_msg, sizeof(recv_msg.txt), 1, 0);
        printf("OTP generator received workspace name from log in: %s\n", recv_msg.txt);

        // generate otp using pid
        int otp = getpid();
        char otp_str[10];
        sprintf(otp_str, "%d", otp);

        // send to login
        struct msg otp_msg;
        otp_msg.type = 2;
        strncpy(otp_msg.txt, otp_str, 5);
        otp_msg.txt[5] = '\0';
        msgsnd(msqid, &otp_msg, sizeof(otp_msg.txt), 0);
        printf("OTP sent to log in from OTP generator: %s\n", otp_msg.txt);

        // send to mail
        otp_msg.type = 3;
        msgsnd(msqid, &otp_msg, sizeof(otp_msg.txt), 0);
        printf("OTP sent to mail from OTP generator: %s\n", otp_msg.txt);

        pid_t mail_pid = fork();
        
        if (mail_pid == 0) {
            // mail process
            struct msg mail_recv;
            msgrcv(msqid, &mail_recv, sizeof(mail_recv.txt), 3, 0);
            printf("Mail received OTP from OTP generator: %s\n", mail_recv.txt);

            // forward to login
            struct msg mail_send;
            mail_send.type = 4;
            strncpy(mail_send.txt, mail_recv.txt, 5);
            mail_send.txt[5] = '\0';
            msgsnd(msqid, &mail_send, sizeof(mail_send.txt), 0);
            printf("OTP sent to log in from mail: %s\n", mail_recv.txt);

            exit(0);
        } else {
            wait(NULL);
            exit(0);
        }
    } else {
        // login process
        wait(NULL);

        // receive from otp generator
        struct msg from_otp;
        msgrcv(msqid, &from_otp, sizeof(from_otp.txt), 2, 0);
        printf("Log in received OTP from OTP generator: %s\n", from_otp.txt);

        // receive from mail
        struct msg from_mail;
        msgrcv(msqid, &from_mail, sizeof(from_mail.txt), 4, 0);
        printf("Log in received OTP from mail: %s\n", from_mail.txt);

        // compare
        if (strcmp(from_otp.txt, from_mail.txt) == 0) {
            printf("OTP Verified\n");
        } else {
            printf("OTP Incorrect\n");
        }

        msgctl(msqid, IPC_RMID, NULL);
    }

    return 0;
}