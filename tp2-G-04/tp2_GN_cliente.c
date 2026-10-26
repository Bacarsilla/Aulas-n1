#define _POSIX_C_SOURCE 200809L
#include "tp2_GN_ipc.h"
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <unistd.h>

static volatile sig_atomic_t notificado = 0;
static void tratar_sinal(int sig) { (void)sig; notificado = 1; }

static void erro(const char *msg) { perror(msg); exit(EXIT_FAILURE); }

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "Uso: %s SHMID MSGID ID_UTILIZADOR\n", argv[0]);
        return EXIT_FAILURE;
    }
    int shmid = atoi(argv[1]), msgid = atoi(argv[2]), id = atoi(argv[3]);
    if (id < 1 || id > TP2_NUM_CLIENTES) {
        fprintf(stderr, "ID de utilizador inválido: %d\n", id);
        return EXIT_FAILURE;
    }
    estado_partilhado *estado = shmat(shmid, NULL, 0);
    if (estado == (void *)-1) erro("cliente: shmat");
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa)); sa.sa_handler = tratar_sinal; sigemptyset(&sa.sa_mask);
    if (sigaction(SIGUSR1, &sa, NULL) == -1) erro("cliente: sigaction");

    email_local enviados[TP2_MAX_CAIXA], recebidos[TP2_MAX_CAIXA];
    int n_enviados = 0, n_recebidos = 0;
    int destinatario = (id % TP2_NUM_CLIENTES) + 1;
    mensagem_ipc msg;
    memset(&msg, 0, sizeof(msg)); msg.mtype = TP2_TIPO_ENTRADA;
    msg.remetente = id; msg.destinatario = destinatario;
    snprintf(msg.texto, sizeof(msg.texto), "Mensagem de utilizador%d para utilizador%d", id, destinatario);

    /* Barreira de prontidão: garante que o handler SIGUSR1 já está instalado. */
    mensagem_ipc pronto; memset(&pronto, 0, sizeof(pronto));
    pronto.mtype = TP2_TIPO_ENTRADA; pronto.remetente = 0; pronto.destinatario = id;
    snprintf(pronto.texto, sizeof(pronto.texto), "READY");
    if (msgsnd(msgid, &pronto, sizeof(pronto) - sizeof(long), 0) == -1) erro("cliente: msgsnd READY");

    /* Caixa de enviados local: regista-se antes de submeter a mensagem ao servidor. */
    enviados[n_enviados].remetente = id;
    enviados[n_enviados].destinatario = destinatario;
    snprintf(enviados[n_enviados++].texto, TP2_TAM_EMAIL, "%s", msg.texto);
    printf("[cliente %d | PID %ld] A enviar para utilizador%d: %s\n", id, (long)getpid(), destinatario, msg.texto);
    if (msgsnd(msgid, &msg, sizeof(msg) - sizeof(long), 0) == -1) erro("cliente: msgsnd");

    /* O servidor envia SIGUSR1 e coloca o email na fila específica deste destinatário. */
    mensagem_ipc recebido;
    ssize_t n;
    do { n = msgrcv(msgid, &recebido, sizeof(recebido) - sizeof(long), (long)id + 1L, 0); }
    while (n == -1 && errno == EINTR);
    if (n == -1) erro("cliente: msgrcv");
    if (n_recebidos < TP2_MAX_CAIXA) {
        recebidos[n_recebidos].remetente = recebido.remetente;
        recebidos[n_recebidos].destinatario = recebido.destinatario;
        snprintf(recebidos[n_recebidos++].texto, TP2_TAM_EMAIL, "%s", recebido.texto);
    }
    printf("[cliente %d | PID %ld] Sinal SIGUSR1 recebido; email recebido de utilizador%d: %s\n",
           id, (long)getpid(), recebido.remetente, recebido.texto);
    printf("[cliente %d] Caixa enviados=%d; caixa recebidos=%d\n", id, n_enviados, n_recebidos);
    if (shmdt(estado) == -1) erro("cliente: shmdt");
    return EXIT_SUCCESS;
}
