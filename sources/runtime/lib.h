#ifndef __SYS_LIB_H_
#define	__SYS_LIB_H_ 1
#ifdef __cplusplus
extern "C" {
#endif

#include <runtime/sys/uosc.h>

#include <runtime/arch.h>
#include <runtime/byteorder.h>
#include <runtime/assert.h>
#include <runtime/list.h>

unsigned long khz();

#ifndef __LINUX__
void qsort (void *a, size_t n, size_t es,
	int (*cmp)(const void*, const void*));
void *bsearch (const void *key, const void *base, size_t nmemb, size_t size,
	int (*compar) (const void *, const void *));
#endif /* __LINUX__ */

#ifdef __GNUC__
#	define alloca(size)	__builtin_alloca (size)
#endif

extern int setjmp (jmp_buf);
extern void longjmp (jmp_buf, int);

/*
 * Debugging console interface.
 */
extern struct _stream_t debug;
// !!! thi is control EOL behaviour.
//\value = 0 - no \n transforms
//\value != 0 - generates \r after \n
extern bool_t debug_onlcr;
unsigned short debug_getchar (void) __cpp_decls;
int debug_peekchar (void) __cpp_decls;

#ifndef NO_DEBUG_PRINT

void debug_putchar (void *arg, short c) __cpp_decls;
void debug_putc (char c) __cpp_decls;
void debug_puts (const char *str) __cpp_decls;
int debug_printf (const char *fmt, ...) __cpp_decls;
int debug_vprintf (const char *fmt, va_list args) __cpp_decls;
void debug_dump (const char *caption, void* data, unsigned len) __cpp_decls;
#ifndef ARCH_debug_dump_stack
void debug_dump_stack (const char *caption, void *sp, void* frame, void *callee);
#else
#define debug_dump_stack ARCH_debug_dump_stack
#endif
void debug_redirect (void (*func) (void*, short), void *arg) __cpp_decls;

#else

INLINE
void debug_putchar (void *arg, short c){};

INLINE
void debug_putc (char c){};

INLINE
void debug_puts (const char *str){};

INLINE
int debug_printf (const char *fmt, ...){return 0;};

INLINE
int debug_vprintf (const char *fmt, va_list args){return 0;};

INLINE
void uos_debug_dump(){};

INLINE
void debug_dump (const char *caption, void* data, unsigned len){};

INLINE
void debug_dump_stack (const char *caption, void *sp, void* frame, void *callee){};

INLINE
void debug_redirect (void (*func) (void*, short), void *arg){};

#endif

/*
 * Call global C++ constructors.
 * This must be done from user code after initializing a memory allocation pool.
 */
void uos_call_global_initializers (void);

/*
 * Call global C++ destructors.
 * Should be called from user code when the system is halted.
 */
void uos_call_global_destructors (void);

/*
 * Delay functions, useful for small pauses.
 */
void udelay (small_uint_t) __cpp_decls;
void mdelay (small_uint_t) __cpp_decls;

/*
 * Check memory address.
 */
bool_t uos_valid_memory_address (const void*);
bool_t uos_valid_code_address (const void*);

/*
 * Halt the system.
 * !! on MIPS cant invoke from exception
 */
//* noreturn modifier breaks an stack contents, so debuger cant show call-stack
#ifdef NDEBUG
#define DEBUG_NORETURN __NORETURN
#else
#define DEBUG_NORETURN
#endif

void rt_halt (int dump_flag) DEBUG_NORETURN ;
void uos_halt (int) DEBUG_NORETURN ;

#ifdef RT_HALT
#define uos_halt(dump_flag) rt_halt(dump_flag)
#endif

#ifndef __AVR__
INLINE unsigned
strlen_flash (const char *str)
{
	return (unsigned) strlen ((const unsigned char*) str);
}

INLINE void
memcpy_flash (void *dest, const char *src, unsigned char len)
{
	memcpy (dest, src, len);
}

INLINE void
strcpy_flash (unsigned char *dest, const char *str)
{
	strcpy (dest, (const unsigned char*) str);
}

INLINE void
strncpy_flash (unsigned char *dest, const char *str, unsigned char maxlen)
{
	strncpy (dest, (const unsigned char*) str, maxlen);
}
#endif /* __AVR__ */

INLINE unsigned char
flash_fetch (const unsigned char *p)
{
	return FETCH_BYTE (p);
}

#ifdef __cplusplus
}
#endif
#endif /* !__SYS_LIB_H_ */
