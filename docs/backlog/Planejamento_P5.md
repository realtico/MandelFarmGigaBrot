# Planejamento P5 - Multicore Local

O P5 deve ser tratado como pacote de aprendizado, nao apenas como uma troca de implementacao. A ideia e sair do render monocore atual com calma, criando primeiro as divisoes de trabalho, depois threads, depois benchmark comparativo, e so entao preparar tiles para a futura distribuicao.

## Visao Geral

Hoje o projeto faz:

```text
1 imagem
1 buffer
1 thread
1 chamada mandel_render_f64()
```

No P5 queremos chegar em:

```text
1 imagem
1 buffer compartilhado
N threads
cada thread calcula uma parte independente
resultado final identico ao single-core
benchmark mede speedup
```

O ponto central e que Mandelbrot e altamente paralelizavel: cada pixel pode ser calculado de forma independente. Isso permite dividir uma imagem em linhas, faixas ou tiles sem mudar o resultado matematico.

## P5.1 - Preparar Render Por Regiao

Antes de criar threads, precisamos ensinar o renderizador a calcular apenas um pedaco da imagem.

Status atual:

```text
P5-001a concluido: mandel_render_region_f64 foi criada.
P5-001b concluido: testes de equivalencia foram adicionados.
```

Assinatura sugerida:

```c
int mandel_render_region_f64(
    const MandelView *view,
    uint32_t *iterations,
    int y_start,
    int y_end
);
```

Essa funcao renderiza apenas as linhas no intervalo:

```text
y_start <= y < y_end
```

Aprendizados:

- o que e uma regiao de trabalho;
- por que linhas independentes nao causam conflito;
- como manter o mesmo mapeamento pixel -> plano complexo;
- como preservar compatibilidade com `mandel_render_f64`;
- por que preparar a divisao de trabalho antes de criar threads reduz complexidade.

Criterio de aceite:

```text
mandel_render_region_f64(view, buffer, 0, height)
```

deve produzir o mesmo resultado que:

```text
mandel_render_f64(view, buffer)
```

## P5.2 - Multithread Por Faixas Horizontais

Depois que renderizar uma regiao funcionar em modo single-thread, podemos dividir a imagem em faixas horizontais.

Status atual:

```text
P5-002a concluido: mandel_render_f64_threads foi criada com pthreads.
P5-002b concluido: comentarios didaticos sobre faixas e ausencia de data race foram adicionados.
```

Exemplo com 4 threads:

```text
thread 0: linhas 0..191
thread 1: linhas 192..383
thread 2: linhas 384..575
thread 3: linhas 576..767
```

Assinatura provavel:

```c
int mandel_render_f64_threads(
    const MandelView *view,
    uint32_t *iterations,
    int thread_count
);
```

Aprendizados:

- criacao de threads com `pthread_create`;
- espera de threads com `pthread_join`;
- struct de argumentos por thread;
- memoria compartilhada;
- por que nao ha data race quando cada thread escreve em linhas diferentes;
- diferenca entre concorrencia e paralelismo;
- custo de criar threads.

Criterios de aceite:

- `thread_count == 1` deve produzir o mesmo resultado do render single-core;
- `thread_count > 1` deve produzir resultado identico ao single-core;
- entradas invalidas devem retornar erro;
- o codigo deve comentar claramente como as faixas sao distribuidas.

## P5.3 - Benchmark Com `--threads`

Depois do render multithread, o `mandel-bench` deve aceitar:

Status atual:

```text
P5-003a concluido: mandel-bench aceita --threads.
P5-003b concluido: saidas humana, JSON e node report registram backend/threads.
```

```sh
./build/bin/mandel-bench --scene medium --threads 4
```

Saida humana esperada:

```text
backend: scalar_f64_threads
threads: 4
duration_ms: ...
pixels_s: ...
iterations_s: ...
```

Saida JSON esperada:

```json
{
  "backend": "scalar_f64_threads",
  "threads": 4
}
```

Aprendizados:

- diferenca entre algoritmo e backend;
- por que benchmark precisa registrar numero de threads;
- comparacao honesta entre `--threads 1` e `--threads N`;
- como medir speedup sem misturar mudancas de algoritmo.

Criterios de aceite:

- `--threads 1` funciona;
- `--threads N` funciona;
- JSON inclui `threads`;
- node capability report passa a refletir threading quando aplicavel.

## P5.4 - Thread Sweep

Depois de suportar `--threads`, adicionamos um modo para comparar varias contagens de threads em uma execucao.

Status atual:

```text
P5-004a concluido: mandel-bench aceita --thread-sweep.
P5-004b concluido: saidas humana e JSON calculam speedup relativo.
```

Exemplo:

```sh
./build/bin/mandel-bench --scene hard --thread-sweep 1,2,4,8
```

Saida humana sugerida:

```text
Threads    Time(ms)    Iter/s       Speedup
1          4200        80 M         1.00x
2          2300        146 M        1.82x
4          1250        269 M        3.36x
8          850         395 M        4.94x
```

Aprendizados:

- speedup;
- overhead de criacao de threads;
- limites de paralelismo;
- diferenca entre cores fisicos e logicos;
- por que `8 threads` nem sempre significa `8x` mais desempenho;
- como workload pequeno pode distorcer benchmark.

Criterios de aceite:

- mede cada configuracao separadamente;
- calcula speedup relativo a 1 thread;
- tem saida humana legivel;
- tem saida JSON opcional.

## P5.5 - Preparar Tiles Locais

Depois das faixas horizontais, introduzimos tiles locais.

Exemplo:

```text
tile 0: x=0   y=0   w=256 h=256
tile 1: x=256 y=0   w=256 h=256
tile 2: x=512 y=0   w=256 h=256
...
```

Aprendizados:

- por que tiles sao melhores para distribuicao futura;
- diferenca entre dividir por linha e dividir por tarefa;
- base para fila de trabalho;
- relacao entre tile local e worker remoto futuro;
- por que tiles tambem ajudam em visualizacao progressiva.

Tipos provaveis:

```c
typedef struct {
    int x;
    int y;
    int width;
    int height;
} MandelTile;
```

Possivel funcao:

```c
int mandel_render_tile_f64(
    const MandelView *view,
    const MandelTile *tile,
    uint32_t *iterations
);
```

Criterios de aceite:

- render por tiles em sequencia produz o mesmo resultado do render completo;
- tiles 64, 128, 256 e 512 sao suportados;
- bordas da imagem funcionam mesmo quando a resolucao nao e multipla do tamanho do tile.

## P5.6 - Fila De Tiles Concorrente

Esta parte pode ficar como extensao do P5 se quisermos entrar em sincronizacao.

Ideia:

```text
varias threads
uma fila compartilhada de tiles
cada thread pega o proximo tile disponivel
renderiza
volta para pegar outro
```

Aprendizados:

- mutex;
- regiao critica;
- fila compartilhada;
- balanceamento dinamico de carga;
- por que tiles podem distribuir trabalho melhor que faixas fixas.

Essa etapa ja comeca a se aproximar da arquitetura futura de workers remotos.

## Backlog Replanejado

```text
P5-001a: renderizacao por regiao single-thread
P5-001b: testes de equivalencia regiao vs render completo

P5-002a: renderizacao multithread por faixas horizontais
P5-002b: comentarios didaticos sobre pthreads e ausencia de data race

P5-003a: mandel-bench --threads
P5-003b: JSON com threads/backend

P5-004a: thread sweep
P5-004b: calculo de speedup

P5-005a: tipos basicos de tile
P5-005b: renderizacao local por tiles em sequencia
P5-005c: equivalencia tile vs render completo

P5-006a: fila local de tiles
P5-006b: threads consumindo tiles de uma fila compartilhada
```

## Proximo Passo Recomendado

Seguir para **P5-005a - tipos basicos de tile**.

Motivo:

- `mandel_render_f64_threads` ja existe e produz resultado equivalente ao single-core;
- os testes cobrem `thread_count` 1, 2, 3 e mais threads que linhas;
- `mandel-bench --threads` ja permite medir uma configuracao por vez;
- `mandel-bench --thread-sweep` ja compara varias configuracoes em uma mesma execucao;
- o proximo foco de aprendizado passa a ser tiles como unidade de trabalho mais flexivel.
