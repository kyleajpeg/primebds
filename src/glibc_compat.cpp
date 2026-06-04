/// @file glibc_compat.cpp
/// glibc 2.38+ compatibility shims for __isoc23_* symbols.
///
/// When libc++.a is compiled on glibc 2.38+ (Ubuntu 24.04+), its objects contain
/// unversioned undefined references to __isoc23_strtol, __isoc23_wcstol, etc. —
/// C23 function names that glibc 2.38+ headers redirect strtol/wcstol/sscanf to.
/// On older hosts (Ubuntu 22.04 / glibc 2.35) those symbols don't exist, so the
/// plugin fails to load with "version `GLIBC_2.38' not found".
///
/// Fix: provide local definitions for all __isoc23_* functions.  Plain (unversioned)
/// definitions satisfy libc++.a's unversioned references, taking priority over
/// glibc's versioned __isoc23_strtol@@GLIBC_2.38 dynamic export.  No GLIBC_2.38
/// entry is created in the final .so's dynamic symbol table.
///
/// Note: system headers must be included BEFORE the __GLIBC_MINOR__ check because
/// __GLIBC__ / __GLIBC_MINOR__ are header macros (from <features.h>), not compiler
/// predefined macros — they are not available until a glibc header is included.

#include <cstdarg>
#include <cwchar>
#include <locale.h> // pulls in <features.h> → defines __GLIBC__ / __GLIBC_MINOR__
#include <stdlib.h>
#include <wchar.h>
#include <stdio.h>

// Only needed when building on glibc 2.38+; compiles to nothing on older build hosts
// or non-Linux platforms.
#if defined(__linux__) && defined(__GLIBC__) && \
    (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 38))

// Instead of using asm labels to bind to the pre-C23 symbol names (which
// triggers inline-assembly-like constructs), resolve the base C functions at
// runtime via `dlsym(RTLD_DEFAULT, "strtol")` etc.  This avoids emitting
// asm labels in the source while still obtaining the original un-macro'd
// symbols that libc++ expects when linking.
#include <dlfcn.h>

namespace {
    template<typename Fn>
    inline Fn resolve_symbol(const char *name) {
        void *sym = dlsym(RTLD_DEFAULT, name);
        return reinterpret_cast<Fn>(sym);
    }

    using fp_strtol = long (*)(const char *, char **, int);
    using fp_strtoul = unsigned long (*)(const char *, char **, int);
    using fp_strtoll = long long (*)(const char *, char **, int);
    using fp_strtoull = unsigned long long (*)(const char *, char **, int);
    using fp_strtoll_l = long long (*)(const char *, char **, int, locale_t);
    using fp_strtoull_l = unsigned long long (*)(const char *, char **, int, locale_t);
    using fp_wcstol = long (*)(const wchar_t *, wchar_t **, int);
    using fp_wcstoul = unsigned long (*)(const wchar_t *, wchar_t **, int);
    using fp_wcstoll = long long (*)(const wchar_t *, wchar_t **, int);
    using fp_wcstoull = unsigned long long (*)(const wchar_t *, wchar_t **, int);
    using fp_vsscanf = int (*)(const char *, const char *, va_list);

    inline fp_strtol c17_strtol() { static fp_strtol p = resolve_symbol<fp_strtol>("strtol"); return p; }
    inline fp_strtoul c17_strtoul() { static fp_strtoul p = resolve_symbol<fp_strtoul>("strtoul"); return p; }
    inline fp_strtoll c17_strtoll() { static fp_strtoll p = resolve_symbol<fp_strtoll>("strtoll"); return p; }
    inline fp_strtoull c17_strtoull() { static fp_strtoull p = resolve_symbol<fp_strtoull>("strtoull"); return p; }
    inline fp_strtoll_l c17_strtoll_l() { static fp_strtoll_l p = resolve_symbol<fp_strtoll_l>("strtoll_l"); return p; }
    inline fp_strtoull_l c17_strtoull_l() { static fp_strtoull_l p = resolve_symbol<fp_strtoull_l>("strtoull_l"); return p; }
    inline fp_wcstol c17_wcstol() { static fp_wcstol p = resolve_symbol<fp_wcstol>("wcstol"); return p; }
    inline fp_wcstoul c17_wcstoul() { static fp_wcstoul p = resolve_symbol<fp_wcstoul>("wcstoul"); return p; }
    inline fp_wcstoll c17_wcstoll() { static fp_wcstoll p = resolve_symbol<fp_wcstoll>("wcstoll"); return p; }
    inline fp_wcstoull c17_wcstoull() { static fp_wcstoull p = resolve_symbol<fp_wcstoull>("wcstoull"); return p; }
    inline fp_vsscanf c17_vsscanf() { static fp_vsscanf p = resolve_symbol<fp_vsscanf>("vsscanf"); return p; }
}

// Provide unversioned definitions for every __isoc23_* symbol pulled in by libc++.a.
// Being unversioned they satisfy libc++.a's unversioned undefs locally, so the
// linker never adds a GLIBC_2.38 dynamic requirement.
// visibility("hidden") keeps them out of the .so's exported symbol table.
#define ISOC23(ret, name, params, args, base_fn)                         \
    extern "C" __attribute__((visibility("hidden"))) ret name params { \
        auto f = base_fn();                                                  \
        if (f) return f args;                                                \
        /* Fallback: call the macro-expanded name as a last resort. */       \
        return ret(); /* zero-initialize on failure to avoid UB */           \
    }

ISOC23(long, __isoc23_strtol, (const char *s, char **e, int b), (s, e, b), c17_strtol)
ISOC23(unsigned long, __isoc23_strtoul, (const char *s, char **e, int b), (s, e, b), c17_strtoul)
ISOC23(long long, __isoc23_strtoll, (const char *s, char **e, int b), (s, e, b), c17_strtoll)
ISOC23(unsigned long long, __isoc23_strtoull, (const char *s, char **e, int b), (s, e, b), c17_strtoull)
ISOC23(long long, __isoc23_strtoll_l, (const char *s, char **e, int b, locale_t l), (s, e, b, l), c17_strtoll_l)
ISOC23(unsigned long long, __isoc23_strtoull_l, (const char *s, char **e, int b, locale_t l), (s, e, b, l), c17_strtoull_l)
ISOC23(long, __isoc23_wcstol, (const wchar_t *s, wchar_t **e, int b), (s, e, b), c17_wcstol)
ISOC23(unsigned long, __isoc23_wcstoul, (const wchar_t *s, wchar_t **e, int b), (s, e, b), c17_wcstoul)
ISOC23(long long, __isoc23_wcstoll, (const wchar_t *s, wchar_t **e, int b), (s, e, b), c17_wcstoll)
ISOC23(unsigned long long, __isoc23_wcstoull, (const wchar_t *s, wchar_t **e, int b), (s, e, b), c17_wcstoull)
ISOC23(int, __isoc23_vsscanf, (const char *s, const char *f, va_list ap), (s, f, ap), c17_vsscanf)

// Variadic — handle separately.
extern "C" __attribute__((visibility("hidden"))) int __isoc23_sscanf(const char *s, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = __c17_vsscanf(s, fmt, ap);
    va_end(ap);
    return r;
}

#endif // linux + glibc 2.38+
