Detecção de deadlocks e violação da ordem de aquisição de travas

**Objetivo**

O objetivo deste projeto é estender uma implementação didática de mutexes feita em sala, adicionando mecanismos de detecção de deadlocks e/ou verificação da ordem de aquisição de travas, inspirando-se em abordagens reais como o lockdep do Linux.

**Descrição**

Mutexes garantem exclusão mútua em regiões críticas, mas quando múltiplas threads acessam múltiplos mutexes simultaneamente, podem surgir situações de deadlock — geralmente causadas por espera circular entre threads. Este projeto propõe duas abordagens complementares:

1. Detecção de deadlocks via grafo de espera:

  ○ Monitorar quais threads possuem e quais esperam por cada mutex.

  ○ Construir dinamicamente um grafo de espera por recursos.

  ○ Detectar ciclos no grafo, sinalizando um deadlock real.

2. Detecção de violação da ordem de aquisição de travas (estilo lockdep):

  ○ Manter o histórico da ordem em que mutexes são adquiridos por cada thread.

  ○ Detectar quando uma thread tenta adquirir mutexes em uma ordem incompatível com a previamente observada, antes que o deadlock ocorra.

  ○ Exibir alertas sobre inversões de ordem (e.g., thread A adquire A → B, e thread B tenta B → A).

**Requisitos**

● Utilizar a implementação de mutex em C discutida em sala como base.

● Estender o mutex com estruturas auxiliares para rastrear:

1. Dono atual.
2. Fila de espera.
3. Ordem de aquisição por thread.

● Implementar pelo menos uma das duas abordagens a seguir (vale pontos extras implementar ambas):

1. Grafo de espera com detecção de ciclos (usando DFS ou algoritmo equivalente).

2. Verificação de ordem de aquisição com comparação entre pares de mutexes adquiridos por diferentes threads.
