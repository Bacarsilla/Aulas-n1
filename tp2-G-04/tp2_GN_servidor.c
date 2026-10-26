#define _POSIX_C_SOURCE 200809L
#include "tp2_GN_ipc.h"
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <unistd.h>

static int shmid = -1, msgid = -1;
static estado_partilhado *estado = (void *)-1;
static void limpar(void) {
    if (estado != (void *)-1) { shmdt(estado); estado = (void *)-1; }
    if (shmid != -1) { shmctl(shmid, IPC_RMID, NULL); shmid = -1; }
    if (msgid != -1) { msgctl(msgid, IPC_RMID, NULL); msgid = -1; }
}
static void erro_fatal(const char *s) { perror(s); limpar(); exit(EXIT_FAILURE); }

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    shmid = shmget(IPC_PRIVATE, sizeof(estado_partilhado), IPC_CREAT | 0600);
    if (shmid == -1) erro_fatal("servidor: shmget");
    estado = shmat(shmid, NULL, 0);
    if (estado == (void *)-1) erro_fatal("servidor: shmat");
    memset(estado, 0, sizeof(*estado)); estado->servidor_ativo = 1;
    msgid = msgget(IPC_PRIVATE, IPC_CREAT | 0600);
    if (msgid == -1) erro_fatal("servidor: msgget");

    printf("=== Servidor de email local (demonstração) ===\n");
    printf("Memória partilhada criada: shmid=%d\nFila de mensagens criada: msgid=%d\n", shmid, msgid);
    const char *programa = "./tp2_GN_cliente";
    for (int id = 1; id <= TP2_NUM_CLIENTES; id++) {
        pid_t pid = fork();
        if (pid == -1) erro_fatal("servidor: fork");
        if (pid == 0) {
            char a[32], b[32], c[32];
            snprintf(a, sizeof(a), "%d", shmid); snprintf(b, sizeof(b), "%d", msgid);
            snprintf(c, sizeof(c), "%d", id);
            execl(programa, programa, a, b, c, (char *)NULL);
            perror("servidor: execl cliente"); _exit(127);
        }
        estado->pids[id] = pid;
        printf("Servidor criou utilizador%d (PID %ld)\n", id, (long)pid);
    }
    estado->clientes_prontos = TP2_NUM_CLIENTES;

    /* Espera até todos os clientes terem instalado o handler de sinais. */
    int prontos = 0, pendentes = 0;
    mensagem_ipc guardadas[TP2_NUM_CLIENTES];
    while (prontos < TP2_NUM_CLIENTES) {
        mensagem_ipc msg;
        ssize_t n;
        do { n = msgrcv(msgid, &msg, sizeof(msg) - sizeof(long), TP2_TIPO_ENTRADA, 0); }
        while (n == -1 && errno == EINTR);
        if (n == -1) erro_fatal("servidor: msgrcv prontidão");
        if (msg.remetente == 0 && strcmp(msg.texto, "READY") == 0) prontos++;
        else if (pendentes < TP2_NUM_CLIENTES) guardadas[pendentes++] = msg;
    }
    printf("Servidor: %d clientes prontos; a encaminhar mensagens.\n", prontos);
    int encaminhadas = 0;
    while (encaminhadas < TP2_NUM_CLIENTES) {
        mensagem_ipc msg;
        if (pendentes > 0) msg = guardadas[--pendentes];
        else {
            ssize_t n;
            do { n = msgrcv(msgid, &msg, sizeof(msg) - sizeof(long), TP2_TIPO_ENTRADA, 0); }
            while (n == -1 && errno == EINTR);
            if (n == -1) erro_fatal("servidor: msgrcv entrada");
            if (msg.remetente == 0) continue;
        }
        if (msg.destinatario < 1 || msg.destinatario > TP2_NUM_CLIENTES) {
            fprintf(stderr, "Destinatário inválido: %d\n", msg.destinatario); continue;
        }
        msg.mtype = (long)msg.destinatario + 1L;
        if (msgsnd(msgid, &msg, sizeof(msg) - sizeof(long), 0) == -1) erro_fatal("servidor: msgsnd encaminhamento");
        printf("Servidor encaminhou utilizador%d -> utilizador%d: %s\n", msg.remetente, msg.destinatario, msg.texto);
        if (kill(estado->pids[msg.destinatario], SIGUSR1) == -1 && errno != ESRCH) erro_fatal("servidor: kill SIGUSR1");
        encaminhadas++;
    }

    int falhas = 0;
    for (int i = 1; i <= TP2_NUM_CLIENTES; i++) {
        int status; pid_t r;
        do { r = waitpid(estado->pids[i], &status, 0); } while (r == -1 && errno == EINTR);
        if (r == -1) { perror("servidor: waitpid"); falhas = 1; }
        else if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) { fprintf(stderr, "Cliente %d terminou com erro\n", i); falhas = 1; }
    }
    estado->servidor_ativo = 0;
    printf("Servidor: todos os clientes terminaram. A remover recursos IPC...\n");
    limpar();
    printf("Servidor terminado; memória partilhada e fila removidas.\n");
    return falhas ? EXIT_FAILURE : EXIT_SUCCESS;
}
