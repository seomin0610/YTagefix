// AgeFix: iOS 26.2 beta 1 has no AgeRangeService.isEligibleForAgeFeatures.
// YouTube weak-links it, gets NULL and crashes (~15s after launch, SIGSEGV at 0x4).
// Fill the NULL __got slots with an async getter that returns false. Real iOS keeps its own symbol.
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

// Swift async `Bool { get async throws }`: x22 = own context ([x22] parent, [x22+8] resume).
// Resume with result in w0, error in x20.
__asm__(
    ".text\n.p2align 2\n"
    "_agefix_false:\n"
    "  ldr x1, [x22, #8]\n"
    "  mov w0, #0\n"
    "  mov x20, #0\n"
    "  br x1\n"
    ".section __TEXT,__const\n.p2align 2\n"
    "_agefix_false_afp:\n"                      // swift AsyncFunctionPointer
    "  .long _agefix_false - _agefix_false_afp\n" // relative function offset
    "  .long 32\n"                                // context size
    ".text\n");
extern char agefix_false[], agefix_false_afp[];

static const struct { const char *name; void *repl; } fixes[] = {
    {"_$s16DeclaredAgeRange0bC7ServiceV013isEligibleForB8FeaturesSbvg", agefix_false},
    {"_$s16DeclaredAgeRange0bC7ServiceV013isEligibleForB8FeaturesSbvgTu", agefix_false_afp},
};

#define NEXT(lc) ((const struct load_command *)((const char *)(lc) + (lc)->cmdsize))

static void on_image(const struct mach_header *mh, intptr_t slide) {
    const struct mach_header_64 *h = (const void *)mh;
    const struct segment_command_64 *linkedit = NULL;
    const struct symtab_command *st = NULL;
    const struct dysymtab_command *dst = NULL;
    const struct load_command *lc = (const void *)(h + 1);
    for (uint32_t i = 0; i < h->ncmds; i++, lc = NEXT(lc)) {
        if (lc->cmd == LC_SEGMENT_64 && !strcmp(((const struct segment_command_64 *)lc)->segname, SEG_LINKEDIT))
            linkedit = (const void *)lc;
        else if (lc->cmd == LC_SYMTAB) st = (const void *)lc;
        else if (lc->cmd == LC_DYSYMTAB) dst = (const void *)lc;
    }
    if (!linkedit || !st || !dst || !dst->nindirectsyms) return;

    uintptr_t le = slide + linkedit->vmaddr - linkedit->fileoff;
    const struct nlist_64 *syms = (const void *)(le + st->symoff);
    const char *strs = (const char *)(le + st->stroff);
    const uint32_t *ind = (const void *)(le + dst->indirectsymoff);
    uintptr_t pg = getpagesize();

    lc = (const void *)(h + 1);
    for (uint32_t i = 0; i < h->ncmds; i++, lc = NEXT(lc)) {
        if (lc->cmd != LC_SEGMENT_64) continue;
        const struct segment_command_64 *seg = (const void *)lc;
        const struct section_64 *sec = (const void *)(seg + 1);
        for (uint32_t j = 0; j < seg->nsects; j++, sec++) {
            if ((sec->flags & SECTION_TYPE) != S_NON_LAZY_SYMBOL_POINTERS) continue;
            void **slot = (void **)(slide + sec->addr);
            for (uint64_t k = 0; k < sec->size / sizeof(void *); k++) {
                uint32_t si = ind[sec->reserved1 + k];
                if (slot[k] || (si & (INDIRECT_SYMBOL_LOCAL | INDIRECT_SYMBOL_ABS))) continue;
                const char *name = strs + syms[si].n_un.n_strx;
                for (size_t f = 0; f < sizeof(fixes) / sizeof(*fixes); f++) {
                    if (strcmp(name, fixes[f].name)) continue;
                    void *page = (void *)((uintptr_t)&slot[k] & ~(pg - 1));
                    mprotect(page, pg, PROT_READ | PROT_WRITE); // __DATA_CONST; left writable
                    slot[k] = fixes[f].repl;
                }
            }
        }
    }
}

__attribute__((constructor)) static void init(void) {
    _dyld_register_func_for_add_image(on_image);
}
