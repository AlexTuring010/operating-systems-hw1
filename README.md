# operating-systems-hw1

A multi-process orchestrator in C demonstrating classic Unix IPC: `fork`, **mmap'd shared memory** (`MAP_SHARED`), and **POSIX semaphores** (`sem_init`, `sem_wait`, `sem_post`).

First homework of the **Operating Systems** course at the University of Athens (Department of Informatics & Telecommunications). Solo project.

## What it does

The parent process reads a list of instructions from a config file. Each instruction tells it to spawn child processes that consume lines from a shared text file. The parent and children coordinate through:

- **A shared-memory region** (`mmap` with `MAP_SHARED`) holding the active line buffer, a command flag, and a step counter.
- **POSIX semaphores** living inside that shared region — one write semaphore (only one writer at a time, the parent), and per-child read semaphores (the parent picks one child per step and signals just that one).
- **Process tracking arrays** (`process_info`, `semaphore_info`) maintained only in the parent, since the parent owns lifecycle.

```
config.txt ──┐
text.txt ────┴──▶ parent
                    │  fork()
                    │  mmap(MAP_SHARED) → SharedMemory
                    │  sem_init() × max_semaphores
                    │
                    ▼
              ┌─────────────┐  ┌─────────────┐  ┌─────────────┐
              │  child 1    │  │  child 2    │  │  child 3    │
              │ sem_wait    │  │ sem_wait    │  │ sem_wait    │
              │  on read[i] │  │  on read[i] │  │  on read[i] │
              └─────────────┘  └─────────────┘  └─────────────┘
                    ▲
                    │ parent picks one child at random,
                    │ writes a line into shared memory,
                    │ sem_post(read[chosen])
                    │ then waits on its own write sem
                    │
              SharedMemory { data, command, step, read_sems[], write_sem }
```

The parent's job is producer-style: pick a random active child, write the next line into `shared.data`, post the chosen child's read semaphore, wait on the write semaphore. Children are consumer-style: wait on their read semaphore, read `shared.data`, signal `write_sem` when done.

The CLI cap on concurrent semaphores (`max_semaphores`) is enforced by the parent — past it, new spawn requests block until a slot frees. The parent also handles process IDs that exceed `max_semaphores` correctly via the tracking arrays (with `realloc` as needed).

## Build & run

```bash
gcc -o my_program my_program.c -pthread
./my_program ./test_files/config_3_100.txt ./test_files/mobydick.txt 5
```

`config_3_100.txt`, `config_3_1000.txt`, and `config_10_10000.txt` ship as test scenarios. The text file is *Moby-Dick* (`mobydick.txt`) by default.

> **Note:** the program hardcodes `NUM_OF_LINES 22315` (line count of Moby Dick) and `LINE_MAX_SIZE 1024`. Swap text files and you must update those — otherwise the program reads past the file and prints garbage.

## What's interesting

- **Shared memory holds the semaphores themselves**, not just the data. Putting the `sem_t`s inside the `mmap`'d region (as opposed to in the parent's heap) is what lets children inherit and use them directly after `fork`.
- **The producer (parent) deliberately picks children at random** rather than round-robin. Helped exercise the IPC under a less predictable schedule than round-robin would.
- **`realloc` for tracking arrays** when child PIDs exceed the initial `max_semaphores` bound — small but easy-to-get-wrong.

## Artefacts

- **`sdi2200284_presentation.pdf`** — the slides used for the in-class presentation of this assignment.

## License

[MIT](LICENSE) — applies to my own code in this repo. *Moby-Dick* is in the public domain.
