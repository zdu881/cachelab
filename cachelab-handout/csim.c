/* Simple cache simulator for the Cache Lab assignment.
 * Implements an S-set, E-line per set, B-block size cache with LRU.
 * Supports command-line flags: -s, -E, -b, -t, -v (verbose), -h (help).
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <limits.h>
#include "cachelab.h"

typedef struct {
    int valid;
    unsigned long long tag;
    unsigned long long lru; /* larger counter for LRU */
} cache_line_t;

static void usage(char *progname)
{
    printf("Usage: %s [-hv] -s <s> -E <E> -b <b> -t <tracefile>\n", progname);
    printf("  -h         Print this help message.\n");
    printf("  -v         Optional verbose flag.\n");
    printf("  -s <s>     Number of set index bits.\n");
    printf("  -E <E>     Number of lines per set.\n");
    printf("  -b <b>     Number of block bits.\n");
    printf("  -t <file>  Trace file.\n");
}

int main(int argc, char *argv[])
{
    int s = -1, E = -1, b = -1;
    char *trace_file = NULL;
    int verbose = 0;
    int opt;

    while ((opt = getopt(argc, argv, "s:E:b:t:vh")) != -1) {
        switch (opt) {
            case 's': s = atoi(optarg); break;
            case 'E': E = atoi(optarg); break;
            case 'b': b = atoi(optarg); break;
            case 't': {
                trace_file = malloc(strlen(optarg) + 1);
                if (trace_file) strcpy(trace_file, optarg);
            } break;
            case 'v': verbose = 1; break;
            case 'h': usage(argv[0]); return 0;
            default: usage(argv[0]); return 1;
        }
    }

    if (s < 0 || E <= 0 || b < 0 || trace_file == NULL) {
        usage(argv[0]);
        return 1;
    }

    unsigned long long S = 1ULL << s;

    /* Allocate cache: array of S sets, each with E lines */
    cache_line_t **cache = malloc(S * sizeof(cache_line_t*));
    if (!cache) {
        perror("malloc");
        return 1;
    }
    for (unsigned long long i = 0; i < S; i++) {
        cache[i] = malloc(E * sizeof(cache_line_t));
        if (!cache[i]) { perror("malloc"); return 1; }
        for (int j = 0; j < E; j++) {
            cache[i][j].valid = 0;
            cache[i][j].tag = 0;
            cache[i][j].lru = 0;
        }
    }

    FILE *fp = fopen(trace_file, "r");
    if (!fp) {
        perror("fopen");
        return 1;
    }

    unsigned int hits = 0, misses = 0, evictions = 0;
    unsigned long long time = 1; /* LRU timestamp counter */

    char op;
    unsigned long long addr;
    int size;

    unsigned long long set_mask = (s == 64) ? ULLONG_MAX : ((1ULL << s) - 1ULL);

    while (fscanf(fp, " %c %llx,%d", &op, &addr, &size) == 3) {
        if (op == 'I') continue; /* ignore instruction loads */

        if (verbose) {
            printf(" %c %llx,%d", op, addr, size);
        }

        /* For 'M', we perform a load then a store (i.e., two accesses) */
        int accesses = (op == 'M') ? 2 : 1;
        for (int a = 0; a < accesses; a++) {
            unsigned long long set_index = (addr >> b) & set_mask;
            unsigned long long tag = addr >> (s + b);

            /* Search for hit */
            int hit_idx = -1;
            for (int i = 0; i < E; i++) {
                if (cache[set_index][i].valid && cache[set_index][i].tag == tag) {
                    hit_idx = i;
                    break;
                }
            }

            if (hit_idx != -1) {
                hits++;
                cache[set_index][hit_idx].lru = time++;
                if (verbose) printf(" hit");
            } else {
                /* Miss: find an empty line first */
                misses++;
                if (verbose) printf(" miss");
                int empty_idx = -1;
                for (int i = 0; i < E; i++) {
                    if (!cache[set_index][i].valid) { empty_idx = i; break; }
                }
                if (empty_idx != -1) {
                    cache[set_index][empty_idx].valid = 1;
                    cache[set_index][empty_idx].tag = tag;
                    cache[set_index][empty_idx].lru = time++;
                } else {
                    /* Eviction: find least recently used line */
                    evictions++;
                    if (verbose) printf(" eviction");
                    unsigned long long min_lru = ULLONG_MAX;
                    int lru_idx = 0;
                    for (int i = 0; i < E; i++) {
                        if (cache[set_index][i].lru < min_lru) {
                            min_lru = cache[set_index][i].lru;
                            lru_idx = i;
                        }
                    }
                    cache[set_index][lru_idx].tag = tag;
                    cache[set_index][lru_idx].lru = time++;
                }
            }
        }

        if (verbose) printf("\n");
    }

    fclose(fp);
    free(trace_file);

    for (unsigned long long i = 0; i < S; i++) free(cache[i]);
    free(cache);

    printSummary(hits, misses, evictions);
    return 0;
}
