#ifndef TP2_GN_IPC_H
#define TP2_GN_IPC_H
#include <sys/types.h>
#define TP2_NUM_CLIENTES 3
#define TP2_TAM_EMAIL 256
#define TP2_MAX_CAIXA 10
#define TP2_TIPO_ENTRADA 1L

typedef struct {
    pid_t pids[TP2_NUM_CLIENTES + 1]; /* índices 1..3 */
    int servidor_ativo;
    int clientes_prontos;
} estado_partilhado;

typedef struct {
    long mtype;
    int remetente;
    int destinatario;
    char texto[TP2_TAM_EMAIL];
} mensagem_ipc;

typedef struct {
    int remetente;
    int destinatario;
    char texto[TP2_TAM_EMAIL];
} email_local;
#endif
