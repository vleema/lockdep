---
title: "Deadlock detection system (lockdep)"
author: Arthur José, Emmanuel Rawa, José Gois, Vinicius Lima, Yuri Santos
theme:
  name: light
---

Sumário
==

<!-- alignment: center -->
<!-- font_size: 2 -->
**Problema**
<!-- pause -->
**Solução**
<!-- pause -->
**Implementação**
<!-- pause -->
**Demonstração**

<!-- end_slide -->
Exemplo do problema
==

<!-- font_size: 2 -->
<!-- alignment: center -->
```plain
Thread A             Thread B
```

<!-- end_slide -->
Exemplo do problema
==

<!-- font_size: 2 -->
<!-- alignment: center -->
```plain
Thread A             Thread B
   |                    |
   | <- 1 (Holds)       | <- 2 (Holds)
```

<!-- end_slide -->
Exemplo do problema
==

<!-- font_size: 2 -->
<!-- alignment: center -->
```plain
Thread A             Thread B
   |                    |
   | <- 1 (Holds)       | <- 2 (Holds)
   |                    | <- 1 (Waits)
```

<!-- end_slide -->
Exemplo do problema
==

<!-- font_size: 2 -->
<!-- alignment: center -->
```plain
Thread A             Thread B
   |                    |
   | <- 1 (Holds)       | <- 2 (Holds)
   |                    | <- 1 (Waits)
   | <- 2 (Waits)       |
   V                    V
```

<!-- font_size: 3 -->
<!-- pause -->
DEADLOCK!

<!-- end_slide -->
Solução
==

<!-- font_size: 3 -->
<!-- alignment: center -->
Criar um grafo que determina a ordem de aquisição de locks entre threads.

<!-- end_slide -->
Solução
==

<!-- alignment: center -->
<!-- font_size: 2 -->
```plain
Thread A             Thread B

Graph:
```
<!-- end_slide -->
Solução
==

<!-- font_size: 2 -->
<!-- alignment: center -->
```plain
Thread A             Thread B
   |                    |
   | <- 1 (Holds)       | <- 2 (Holds)

Graph: 1
       2
```

<!-- end_slide -->
Solução
==

<!-- font_size: 2 -->
<!-- alignment: center -->
```plain
Thread A             Thread B
   |                    |
   | <- 1 (Holds)       | <- 2 (Holds)
   |                    | <- 1 (Waits)
Graph: 1
       2 -> 1
```

<!-- end_slide -->
Solução
==

<!-- font_size: 2 -->
<!-- alignment: center -->
```plain{1-10|8,9}
Thread A             Thread B
   |                    |
   | <- 1 (Holds)       | <- 2 (Holds)
   |                    | <- 1 (Waits)
   | <- 2 (Waits)       |
   V                    V

Graph: 1 -> 2
       2 -> 1
```

<!-- font_size: 3 -->
<!-- pause -->
**DEADLOCK DETECTED!**

<!-- end_slide -->
Plano de Implementação
==

<!-- font_size: 2 -->
1. Armazenar o contexto de cada thread
<!-- pause -->
2. Registrar cada lock no gráfico
<!-- pause -->
3. Detectar ciclo no grafo antes de adquirir a trava
<!-- pause -->
4. Notificar se houver deadlock

<!-- end_slide -->
Contexto da thread
==

<!-- alignment: center -->
<!-- font_size: 2 -->

```c {1-5|7-11|1-11}
typedef struct thread_context {
    pthread_t pthread_id;
    held_lock_t* held_locks;
    struct thread_context* next;
} thread_context_t;

typedef struct held_lock {
    lock_node_t* lock;
    thread_context_t* thread_context;
    struct held_lock* next;
} held_lock_t;
```

<!-- end_slide -->
Estrutura do grafo
==

<!-- alignment: center -->
<!-- font_size: 2 -->

```c {1-6|8-11|1-11}
typedef struct lock_node {
    const void* lock_addr;
    bool was_visited;
    adjacency_locks_t* children;
    struct lock_node* next;
} lock_node_t;

typedef struct adjacent_locks {
    lock_node_t* lock;
    struct adjacent_locks* next;
} adjacency_locks_t;
```

<!-- end_slide -->
API do lockdep
==

<!-- alignment: center -->
<!-- font_size: 2 -->
```c
void lockdep_init(void);

bool lockdep_acquire_lock(const void* lock_addr);

void lockdep_release_lock(const void* lock_addr);

extern bool lockdep_enabled;
```

<!-- end_slide -->
Como usar
==

<!-- font_size: 2 -->
```bash
$ LD_PRELOAD=./liblockdep_interpose.so ./your_program
```
<!-- pause -->
- Pre-load da biblioteca `liblockdep.so` no ínicio do programa usando `LD_PRELOAD`

<!-- end_slide -->
<!-- alignment: center -->
<!-- jump_to_middle -->

<!-- font_size: 4 -->
**Demonstração**
