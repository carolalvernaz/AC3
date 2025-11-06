# Simulador do Algoritmo de Tomasulo

Este projeto é um simulador em C do algoritmo de Tomasulo para escalonamento dinâmico de instruções, estendido com suporte para execução especulativa usando um Buffer de Reordenação (ROB).

## 1\. O Algoritmo de Tomasulo com Especulação

As explicações a seguir sobre o algoritmo são baseadas nos conceitos apresentados no livro *Computer Architecture: A Quantitative Approach*.

O algoritmo de Tomasulo é uma técnica de escalonamento dinâmico que permite a execução de instruções fora de ordem (out-of-order) para maximizar o paralelismo em nível de instrução (ILP).

Este simulador implementa a versão moderna do algoritmo, que o estende com **execução especulativa** baseada em hardware. A combinação dessas técnicas permite ao processador "adivinhar" o resultado de branches (desvios) e executar instruções de caminhos previstos antes que o desvio seja resolvido.

A arquitetura simulada combina três ideias principais:

1.  **Predição Dinâmica de Desvios:** Para escolher quais instruções executar.
2.  **Execução Especulativa:** Permite que as instruções sejam executadas antes que as dependências de controle sejam resolvidas.
3.  **Escalonamento Dinâmico:** Gerencia a execução das instruções assim que seus operandos estão disponíveis.

### O Pipeline de Execução

O fluxo de instruções neste simulador segue quatro estágios principais:

1.  **Issue (Emissão):** A instrução é lida e alocada em uma **Estação de Reserva (RS)** livre e em um slot no **Buffer de Reordenação (ROB)**. Se os operandos estiverem disponíveis (nos registradores ou no ROB), eles são copiados para a RS. Se não, a RS aguarda os resultados.
2.  **Execute (Execução):** A instrução aguarda na RS até que todos os seus operandos estejam disponíveis (monitorando o CDB). Quando prontos, a operação é executada.
3.  **Write Result (Escrita de Resultado):** O resultado da operação é escrito no **Common Data Bus (CDB)**. De lá, ele é enviado para o slot correspondente no ROB e para quaisquer Estações de Reserva que aguardavam por esse resultado.
4.  **Commit (Efetivação):** Este é o estágio final. As instruções são forçadas a "completar" na ordem original do programa. Quando uma instrução chega à frente do ROB e seu resultado está pronto, o resultado é permanentemente escrito no banco de registradores (ou na memória, no caso de um *Store*). Só então a instrução é removida do ROB.

A vantagem principal dessa abordagem é que o estado permanente do processador (registradores e memória) só é atualizado na fase de *Commit*. Isso garante que, se uma especulação (predição de desvio) estiver errada, os resultados incorretos possam ser simplesmente descartados do ROB sem corromper o estado da máquina, permitindo **exceções precisas**.

## 2\. Integrantes do Grupo

  * **Professor:** Matheus Alcântara Souza
  * Caroline Freitas Alvernaz
  * Giovanna Naves Ribeiro
  * Júlia Rodrigues Vasconcellos Melo
  * Marcos Paulo da Silva Laine
  * Priscila Andrade de Moraes

## 3\. Como Compilar e Executar

Este projeto foi escrito em C e pode ser compilado usando `gcc` (ou qualquer compilador C padrão). O simulador lê as instruções de um arquivo chamado `programa2.txt`.

**Compilação:**
(Supondo que o código-fonte se chame `tomasulo.c`)

```bash
gcc -o simulador tomasulo.c
```

**Execução:**

```bash
./simulador
```

*(O programa procurará automaticamente pelo arquivo `programa2.txt` no mesmo diretório.)*

## 4\. Formato do Arquivo de Entrada (`programa2.txt`)

O arquivo `programa2.txt` deve conter uma lista de instruções, uma por linha. As instruções devem usar mnemônicos em MAIÚSCULAS e registradores prefixados com `R`.

O simulador suporta as seguintes operações:

  * **LD (Load Immediate):** Carrega um valor imediato em um registrador.
      * Formato: `LD Rd, R_base, Imediato`
      * Exemplo: `LD R1, R0, 6` (Carrega o valor 6 em R1)
  * **ADD (Adição):** Soma o conteúdo de dois registradores.
      * Formato: `ADD Rd, Rs, Rt`
      * Exemplo: `ADD R3, R1, R2` (R3 = R1 + R2)
  * **SUB (Subtração):** Subtrai o conteúdo de dois registradores.
      * Formato: `SUB Rd, Rs, Rt`
      * Exemplo: `SUB R5, R3, R4` (R5 = R3 - R4)
  * **MUL (Multiplicação):** Multiplica o conteúdo de dois registradores.
      * Formato: `MUL Rd, Rs, Rt`
      * Exemplo: `MUL R4, R1, R2` (R4 = R1 \* R2)
  * **HALT (Parada):** Indica o fim do programa.
      * Formato: `HALT`

### Exemplo de `programa2.txt`

```
LD R1, R0, 6
LD R2, R0, 10
ADD R3, R1, R2
MUL R4, R1, R2
SUB R5, R3, R4
HALT
```

## 5\. Siglas e Conceitos-Chave

  * **RS (Reservation Station / Estação de Reserva):** Um buffer que armazena uma instrução que foi emitida, seus operandos (Vj, Vk) e/ou os "tags" das estações que produzirão os operandos que faltam (Qj, Qk). As estações de reserva permitem o **renome de registradores**, o que elimina conflitos WAW e WAR.
  * **ROB (Reorder Buffer / Buffer de Reordenação):** Um buffer de hardware que armazena os resultados das instruções que terminaram a execução, mas que ainda não foram "cometidas" (efetivadas). É a estrutura central que permite a execução especulativa e garante o *commit* em ordem.
  * **CDB (Common Data Bus / Barramento de Dados Comum):** Um barramento de difusão (broadcast) que transporta os resultados das unidades funcionais (e do ROB) de volta para as estações de reserva e para o ROB.
  * **RAW (Read-After-Write):** Um conflito de "Leitura Após Escrita". Representa uma dependência de dados verdadeira (uma instrução precisa ler um valor que uma instrução anterior ainda não escreveu). O algoritmo gerencia isso fazendo a instrução dependente esperar na estação de reserva pelo resultado (via CDB).
  * **WAR (Write-After-Read):** Um conflito de "Escrita Após Leitura". Ocorre quando uma instrução (ex: B) tenta escrever em um registrador antes que uma instrução anterior (ex: A) tenha lido o valor antigo. Tomasulo resolve isso copiando o valor para a estação de reserva no momento da emissão.
  * **WAW (Write-After-Write):** Um conflito de "Escrita Após Escrita". Ocorre quando uma instrução (ex: B) tenta escrever em um registrador antes que uma instrução anterior (ex: A) tenha escrito seu próprio resultado. Tomasulo resolve isso por meio do renome de registradores nas estações de reserva e no ROB.
  * **Commit (Efetivação):** O estágio final onde o resultado de uma instrução (que está na frente do ROB) é permanentemente escrito no banco de registradores ou na memória. Isso garante que a ordem do programa seja preservada e que as exceções sejam precisas.
