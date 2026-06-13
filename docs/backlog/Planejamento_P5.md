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
P5-004c concluido: mandel-bench aceita --repeat para reduzir ruido de medicao.
```

Exemplo:

```sh
./build/bin/mandel-bench --scene hard --thread-sweep 1,2,4,8 --repeat 5
```

Saida humana sugerida:

```text
Threads    Best(ms)    Avg(ms)     Worst(ms)   Iter/s       Speedup
1          4200        4210        4235        80 M         1.00x
2          2300        2315        2360        146 M        1.82x
4          1250        1280        1320        269 M        3.36x
8          850         880         930         395 M        4.94x
```

Aprendizados:

- speedup;
- overhead de criacao de threads;
- limites de paralelismo;
- diferenca entre cores fisicos e logicos;
- por que `8 threads` nem sempre significa `8x` mais desempenho;
- como workload pequeno pode distorcer benchmark;
- por que repetir medicoes ajuda a separar desempenho real de ruido do sistema operacional;
- diferenca entre melhor tempo, tempo medio e pior tempo.

Criterios de aceite:

- mede cada configuracao separadamente;
- calcula speedup relativo a 1 thread;
- tem saida humana legivel;
- tem saida JSON opcional;
- aceita `--repeat N` para executar cada configuracao varias vezes.

## P5.5 - Preparar Tiles Locais

Depois das faixas horizontais, introduzimos tiles locais.

Status atual:

```text
P5-005a concluido: MandelTile foi criado.
P5-005b concluido: mandel_render_tile_f64 renderiza um retangulo local.
P5-005c concluido: testes provam equivalencia tile vs render completo.
```

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
    uint32_t *iterations,
    const MandelTile *tile
);
```

Criterios de aceite:

- render por tiles em sequencia produz o mesmo resultado do render completo;
- tiles 64, 128, 256 e 512 sao suportados;
- bordas da imagem funcionam mesmo quando a resolucao nao e multipla do tamanho do tile.

Como rodar este marco:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Os tiles tambem aparecem no `mandel-bench` quando usamos o scheduler dinamico:

```sh
./build/bin/mandel-bench \
  --scene hard \
  --thread-sweep 1,2,4,8,10,12,16,20 \
  --scheduler tiles \
  --tile-size 128 \
  --repeat 5
```

## P5.6 - Fila De Tiles Concorrente

Depois de termos tiles locais, introduzimos uma fila compartilhada consumida por varias threads.

Status atual:

```text
P5-006a concluido: fila local de tiles criada dentro do renderer.
P5-006b concluido: threads consomem tiles da fila com mutex.
P5-006c concluido: mandel-bench expoe --scheduler bands|tiles e --tile-size.
```

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
- por que o lock deve proteger apenas a retirada do proximo tile, nao o calculo pesado;
- como comparar scheduler fixo por faixas contra scheduler dinamico por tarefas.

Essa etapa ja comeca a se aproximar da arquitetura futura de workers remotos.

Como rodar este marco:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Comparar faixas fixas:

```sh
./build/bin/mandel-bench \
  --scene hard \
  --thread-sweep 1,2,4,8,10,12,16,20 \
  --scheduler bands \
  --repeat 5
```

Comparar fila dinamica de tiles:

```sh
./build/bin/mandel-bench \
  --scene hard \
  --thread-sweep 1,2,4,8,10,12,16,20 \
  --scheduler tiles \
  --tile-size 128 \
  --repeat 5
```

## P5.7 - Tile Sweep

Depois de comparar schedulers, precisamos estudar a granularidade dos tiles.

Status atual:

```text
P5-007a concluido: mandel-bench aceita --tile-sweep.
P5-007b concluido: saida humana mostra tamanho do tile e quantidade de tiles.
P5-007c concluido: saida JSON registra tile_size, tile_count e speedup relativo.
```

Ideia:

```text
threads fixas
varios tamanhos de tile
mesma cena
mesmo renderer dinamico
comparar overhead vs balanceamento
```

Aprendizados:

- tiles pequenos aumentam a quantidade de trabalhos e o trafego na fila;
- tiles grandes reduzem overhead, mas podem deixar threads ociosas;
- `tile_count` precisa ser grande o suficiente para alimentar as threads;
- o melhor tile depende da cena, resolucao, max_iter e maquina.

Exemplo:

```sh
./build/bin/mandel-bench \
  --scene hard \
  --threads 10 \
  --tile-sweep 16,32,64,128,256,512 \
  --repeat 5
```

## P5.8 - Estudos Locais De Desempenho

Depois de criar as ferramentas, registramos uma primeira rodada de estudo local.

Status atual:

```text
P5-008a concluido: docs/benchmarks criado para estudos reproduziveis.
P5-008b concluido: estudo local Methode registrado em Markdown.
P5-008c concluido: JSONs de node report, thread sweep e tile sweep salvos.
```

## P5.9 - Tile Grid

Para fechar a rodada multicore local, adicionamos um estudo combinado.

Status atual:

```text
P5-009a concluido: mandel-bench aceita --study tile-grid.
P5-009b concluido: --threads-list cruza threads com --tile-sweep.
P5-009c concluido: saida humana mostra matriz tile x threads.
P5-009d concluido: saida JSON registra cada celula da matriz.
```

Exemplo:

```sh
./build/bin/mandel-bench \
  --scene hard \
  --study tile-grid \
  --threads-list 8,10,12,16 \
  --tile-sweep 16,32,64,128 \
  --repeat 5
```

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
P5-004c: repeticao de benchmark com melhor/media/pior tempo

P5-005a: tipos basicos de tile
P5-005b: renderizacao local por tiles em sequencia
P5-005c: equivalencia tile vs render completo

P5-006a: fila local de tiles
P5-006b: threads consumindo tiles de uma fila compartilhada
P5-006c: benchmark comparando scheduler bands vs tiles

P5-007a: mandel-bench aceita --tile-sweep
P5-007b: saida humana mostra tamanho do tile e quantidade de tiles
P5-007c: saida JSON registra tile_size, tile_count e speedup relativo

P5-008a: docs/benchmarks criado para estudos reproduziveis
P5-008b: estudo local Methode registrado em Markdown
P5-008c: JSONs de node report, thread sweep e tile sweep salvos

P5-009a: mandel-bench --study tile-grid
P5-009b: --threads-list para estudos combinados
P5-009c: matriz humana tile x threads
P5-009d: JSON do estudo combinado
```

## Proximo Passo Recomendado

Encerrar o P5 como rodada multicore local e replanejar os proximos epicos.

Motivo:

- ja temos scheduler fixo por faixas;
- ja temos scheduler dinamico por fila de tiles;
- `--tile-sweep` ja mede como `16`, `32`, `64`, `128`, `256` e `512` mudam overhead, balanceamento e throughput;
- o estudo local Methode ja esta registrado em `docs/benchmarks`;
- `--study tile-grid` cruza threads e tiles em uma unica execucao;
- o proximo passo natural e decidir o foco do proximo epico: novos metodos matematicos, backend numerico, visualizacao, ou distribuicao.

Como rodar este marco:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Estudar granularidade de tiles:

```sh
./build/bin/mandel-bench \
  --scene hard \
  --threads 10 \
  --tile-sweep 16,32,64,128,256,512 \
  --repeat 5
```

Gerar JSON para analise posterior:

```sh
./build/bin/mandel-bench \
  --scene hard \
  --threads 10 \
  --tile-sweep 16,32,64,128,256,512 \
  --repeat 5 \
  --json \
  --output assets/output/tile-sweep-hard.json
```

Relatorio local registrado:

```text
docs/benchmarks/p5-008-methode-local.md
docs/benchmarks/results/
```
