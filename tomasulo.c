#include <stdbool.h>
#include <stdio.h>
#include <string.h>

// --- Configuração da Arquitetura ---
#define MAX_INSTR_MEM 10
#define QTD_ESTACOES 4
#define TAM_FILA_ROB 4
#define QTD_REGISTRADORES 8

// --- Estruturas de Dados ---

// Tipos de operação
typedef enum { ADD, SUB, MUL, DIV, LI, HALT } OpType;

// Instrução em "memória"
typedef struct {
    int op;
    int rs1;
    int rs2; // Pode ser rs2 ou imediato
    int rd;
} Operacao;

Operacao memoria_instrucoes[MAX_INSTR_MEM];

// Estação de Reserva (ER)
typedef struct {
    OpType op;
    int tag_j, tag_k; // Tags do ROB (qj, qk)
    int val_j, val_k; // Valores dos operandos (vj, vk)
    int rob_destino;  // Índice do ROB para onde escrever
    bool ocupado;
} SlotReserva;

SlotReserva estacoes_reserva[QTD_ESTACOES];

// Item do Buffer de Reordenação (ROB)
typedef struct {
    OpType op;
    int reg_arq_dest; // Registrador de destino
    int valor;
    bool pronto;      // Resultado está pronto
    bool em_uso;      // Entrada está em uso
} ItemROB;

ItemROB fila_reordenacao[TAM_FILA_ROB];

// Arquivo de Registradores
typedef struct {
    int regs[QTD_REGISTRADORES];
} ArquivoRegs;

ArquivoRegs registradores_arq = {{0}};

// Unidade de Controle (Estado da CPU)
typedef struct {
    int pc;
    int rob_head; // Ponteiro de commit
    int rob_tail; // Ponteiro de issue
    int rob_contagem; // Ocupação
    int ciclo;
} UnidadeControle;

UnidadeControle cpu_core = {0, 0, 0, 0, 1}; // Começa ciclo 1

// --- Funções Auxiliares ---

bool rob_cheio() {
    return cpu_core.rob_contagem >= TAM_FILA_ROB;
}

int encontrar_er_livre() {
    for (int i = 0; i < QTD_ESTACOES; i++) {
        if (!estacoes_reserva[i].ocupado)
            return i;
    }
    return -1;
}

OpType decodificar_mnemonico(const char *mnemonic) {
    if (strcmp(mnemonic, "ADD") == 0) return ADD;
    if (strcmp(mnemonic, "MUL") == 0) return MUL;
    if (strcmp(mnemonic, "SUB") == 0) return SUB;
    if (strcmp(mnemonic, "DIV") == 0) return DIV;
    if (strcmp(mnemonic, "LD") == 0) return LI; 
    if (strcmp(mnemonic, "HALT") == 0) return HALT;
    fprintf(stderr, "Instrucao desconhecida: %s\n", mnemonic);
    return HALT;
}

// --- Funções de Impressão ---

void mostrar_banco_regs() {
    printf("Registradores: ");
    for (int i = 0; i < QTD_REGISTRADORES; i++) {
        printf("R%d = %d", i, registradores_arq.regs[i]);
        if (i < QTD_REGISTRADORES - 1) {
            printf(", ");
        }
    }
    printf("\n");
}

void mostrar_regs_final() {
    printf("Registradores: ");
     for (int i = 0; i < QTD_REGISTRADORES; i++) {
        printf("R%d = %d ", i, registradores_arq.regs[i]);
    }
    printf("\n");
}

// === Estágios do Pipeline ===

// Estágio 1: Despacho (Issue)
void etapa_despacho() {
    Operacao instr_atual = memoria_instrucoes[cpu_core.pc];
    if (instr_atual.op == HALT) {
        return;
    }
    
    int er_idx = encontrar_er_livre();

    if (rob_cheio()) {
        printf("Stall: ROB cheio.\n");
        return; 
    }
    if (er_idx == -1) {
        printf("Stall: Estacoes de reserva cheias.\n");
        return;
    }

    // Aloca entrada no ROB e na ER
    int rob_idx = cpu_core.rob_tail;
    fila_reordenacao[rob_idx].op = instr_atual.op;
    fila_reordenacao[rob_idx].reg_arq_dest = instr_atual.rd;
    fila_reordenacao[rob_idx].pronto = false;
    fila_reordenacao[rob_idx].em_uso = true;

    cpu_core.rob_tail = (cpu_core.rob_tail + 1) % TAM_FILA_ROB;
    cpu_core.rob_contagem++;
    
    estacoes_reserva[er_idx].ocupado = true;
    estacoes_reserva[er_idx].op = instr_atual.op;
    estacoes_reserva[er_idx].rob_destino = rob_idx;

    // Busca operandos
    if (instr_atual.op == LI) {
        estacoes_reserva[er_idx].tag_j = -1;
        estacoes_reserva[er_idx].val_j = instr_atual.rs2; // Imediato
        estacoes_reserva[er_idx].tag_k = -1;
        estacoes_reserva[er_idx].val_k = 0;
    } else {
        // Operando J (rs1)
        estacoes_reserva[er_idx].tag_j = -1;
        for (int i = 0; i < TAM_FILA_ROB; i++) {
            if (fila_reordenacao[i].em_uso && fila_reordenacao[i].reg_arq_dest == instr_atual.rs1 && !fila_reordenacao[i].pronto) {
                estacoes_reserva[er_idx].tag_j = i; // Depende do ROB[i]
                break;
            }
        }
        if (estacoes_reserva[er_idx].tag_j == -1)
            estacoes_reserva[er_idx].val_j = registradores_arq.regs[instr_atual.rs1];
        
        // Operando K (rs2)
        estacoes_reserva[er_idx].tag_k = -1;
        for (int i = 0; i < TAM_FILA_ROB; i++) {
            if (fila_reordenacao[i].em_uso && fila_reordenacao[i].reg_arq_dest == instr_atual.rs2 && !fila_reordenacao[i].pronto) {
                estacoes_reserva[er_idx].tag_k = i; // Depende do ROB[i]
                break;
            }
        }
        if (estacoes_reserva[er_idx].tag_k == -1)
            estacoes_reserva[er_idx].val_k = registradores_arq.regs[instr_atual.rs2];
    }
    
    // *** Saída Modificada ***
    const char *op_str = (instr_atual.op == ADD) ? "op" : (instr_atual.op == SUB) ? "op" : (instr_atual.op == MUL) ? "op" : (instr_atual.op == DIV) ? "op" : "<-";
     if (instr_atual.op == LI) {
         printf("Issue: PC=%d -> ER[%d], ROB[%d], R%d = %s %d\n",
           cpu_core.pc, er_idx, rob_idx, instr_atual.rd, op_str, instr_atual.rs2);
    } else {
        printf("Issue: PC=%d -> ER[%d], ROB[%d], R%d = R%d %s R%d\n",
           cpu_core.pc, er_idx, rob_idx, instr_atual.rd, instr_atual.rs1, op_str, instr_atual.rs2);
    }
    
    cpu_core.pc++;
}

// Estágio 2: Execução
void etapa_execucao() {
    for (int i = 0; i < QTD_ESTACOES; i++) {
        SlotReserva *unidade = &estacoes_reserva[i];
        
        if (unidade->ocupado && unidade->tag_j == -1 && unidade->tag_k == -1) {
            int resultado = 0;
            switch (unidade->op) {
                case ADD: resultado = unidade->val_j + unidade->val_k; break;
                case SUB: resultado = unidade->val_j - unidade->val_k; break;
                case MUL: resultado = unidade->val_j * unidade->val_k; break;
                case DIV: resultado = (unidade->val_k ? unidade->val_j / unidade->val_k : 0); break;
                case LI:  resultado = unidade->val_j; break;
                default: break;
            }
            
            fila_reordenacao[unidade->rob_destino].valor = resultado;
            fila_reordenacao[unidade->rob_destino].pronto = true;
            
            // *** Saída Modificada ***
            printf("Execute: ER[%d] (Op: %d) -> ROB[%d] (Resultado: %d)\n", i, unidade->op, unidade->rob_destino, resultado);
            unidade->ocupado = false; // Libera ER
        }
    }
}

// Estágio 3/4: Escrita (CDB) e Finalização (Commit)
void etapa_finalizacao() {
    
    // Parte 1: Escrita no CDB (Broadcast)
    for (int i = 0; i < TAM_FILA_ROB; i++) {
        if (fila_reordenacao[i].em_uso && fila_reordenacao[i].pronto) {
            for (int j = 0; j < QTD_ESTACOES; j++) {
                if (estacoes_reserva[j].ocupado) {
                    if (estacoes_reserva[j].tag_j == i) {
                        estacoes_reserva[j].val_j = fila_reordenacao[i].valor;
                        estacoes_reserva[j].tag_j = -1;
                    }
                    if (estacoes_reserva[j].tag_k == i) {
                        estacoes_reserva[j].val_k = fila_reordenacao[i].valor;
                        estacoes_reserva[j].tag_k = -1;
                    }
                }
            }
        }
    }

    // Parte 2: Commit (em ordem)
    int head_idx = cpu_core.rob_head;
    if (fila_reordenacao[head_idx].em_uso && fila_reordenacao[head_idx].pronto) {
        
        int dest_reg = fila_reordenacao[head_idx].reg_arq_dest;
        int val_final = fila_reordenacao[head_idx].valor;

        // Atualiza Arquivo de Registradores
        registradores_arq.regs[dest_reg] = val_final;
        
        // *** Saída Modificada ***
        printf("Commit: R%d <- %d (ROB[%d])\n", dest_reg, val_final, head_idx);

        // Libera entrada do ROB
        fila_reordenacao[head_idx].em_uso = false;
        fila_reordenacao[head_idx].pronto = false; 

        cpu_core.rob_head = (cpu_core.rob_head + 1) % TAM_FILA_ROB;
        cpu_core.rob_contagem--;
    }
}

// === Main ===

int main() {
    FILE *fp = fopen("simulacao.txt", "r");
    if (fp == NULL) {
        perror("Erro ao abrir 'simulacao.txt'");
        return 1;
    }

    // Carrega instruções
    char line[100];
    int count = 0;
    while (fgets(line, sizeof(line), fp) && count < MAX_INSTR_MEM) {
        char mnemonic[10];
        int rd = 0, rs = 0, rt = 0, imm = 0;

        if (sscanf(line, "%s R%d, R%d, R%d", mnemonic, &rd, &rs, &rt) >= 1 ||
            sscanf(line, "%s R%d, R%d, %d", mnemonic, &rd, &rs, &imm) >= 1) {
            
            Operacao instr;
            instr.op = decodificar_mnemonico(mnemonic);
            
            if (instr.op == LI) {
                if (sscanf(line, "%*s R%d, R%d, %d", &rd, &rs, &imm) == 3) {
                    instr.rd = rd;
                    instr.rs1 = rs;
                    instr.rs2 = imm;
                } else {
                    fprintf(stderr, "Erro ao ler LD: %s\n", line);
                    continue;
                }
            } else if (instr.op != HALT) {
                 sscanf(line, "%*s R%d, R%d, R%d", &rd, &rs, &rt);
                instr.rd = rd;
                instr.rs1 = rs;
                instr.rs2 = rt;
            } else {
                instr.rd = instr.rs1 = instr.rs2 = 0;
            }
            
            memoria_instrucoes[count++] = instr;
            if (instr.op == HALT) {
                break;
            }
        }
    }
    fclose(fp);

    // Loop principal da simulação
    bool halt_detectado = false;
    
    while (true) {
        
        if (memoria_instrucoes[cpu_core.pc].op == HALT) {
            halt_detectado = true;
        }

        // Condição de parada
        if (halt_detectado && cpu_core.rob_contagem == 0) {
            break; 
        }

        printf("Ciclo %d\n", cpu_core.ciclo);
        mostrar_banco_regs();

        if (!halt_detectado) {
            etapa_despacho();
        }
        etapa_execucao();
        etapa_finalizacao();

        cpu_core.ciclo++;
        printf("\n"); 

        if (cpu_core.ciclo > 100) {
            printf("Simulacao excedeu 100 ciclos. Abortando.\n");
            break;
        }
    }

    // Fim
    printf("ESTADO FINAL\n");
    mostrar_regs_final();

    return 0;
}