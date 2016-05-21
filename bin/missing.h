// Function prototypes missing in SDK:
extern int os_printf_plus (const char *format, ...) __attribute__ ((format (printf, 1, 2)));
extern int ets_sprintf(char *str, const char *format, ...)  __attribute__ ((format (printf, 2, 3)));
extern void ets_timer_arm_new (os_timer_t *, uint32_t, uint32_t, uint32_t);
extern void ets_timer_disarm (os_timer_t *);
extern void ets_timer_setfn (os_timer_t *, ETSTimerFunc *, void *);
extern void *ets_memcpy (void *dest, const void *src, size_t n);
extern void *pvPortMalloc (size_t, char *, int);
extern void vPortFree (void *, char *, int);
extern uint16_t readvdd33 (void);

#define os_sprintf	ets_sprintf
