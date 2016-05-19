#include <ets_sys.h>
#include <osapi.h>

// Function prototypes missing in SDK:
extern void uart_div_modify (uint8_t uart, uint32_t freq);
extern void ets_isr_mask (uint32_t intr);
extern void ets_install_putc1 (void *handler);

// Register definitions taken from SDK:
#define REG_UART_BASE(n)	(0x60000000 + (n) * 0xF00)
#define UART_FIFO(n)		(REG_UART_BASE(n) + 0x000)
#define UART_INT_CLR(n)		(REG_UART_BASE(n) + 0x010)
#define UART_STATUS(n)		(REG_UART_BASE(n) + 0x01C)
#define UART_CONF0(n)		(REG_UART_BASE(n) + 0x020)
#define UART_TXFIFO_CNT		0x000000FF
#define UART_TXFIFO_CNT_S	16
#define UART_BIT_NUM_S		2
#define UART_TXFIFO_RST		((uint32_t)1 << 18)

#define UART0			0
#define BAUDRATE		115200

// Number of bytes remaining in Tx FIFO
static inline uint8_t
tx_fifo_count (void)
{
	return (READ_PERI_REG(UART_STATUS(UART0)) >> UART_TXFIFO_CNT_S) & UART_TXFIFO_CNT;
}

// Transmit a single raw character
static void ICACHE_FLASH_ATTR
uart_putc_raw (const uint8_t c)
{
	while (tx_fifo_count() >= 126)
		continue;

	WRITE_PERI_REG(UART_FIFO(UART0), c);
}

// Transmit a single cooked character
static void ICACHE_FLASH_ATTR
uart_putc (const uint8_t c)
{
	if (c == '\n')
		uart_putc_raw('\r');

	uart_putc_raw(c);
}

// Initialize UART0 as Tx-only, 115200 bps, 8N1
void ICACHE_FLASH_ATTR
uart_init (void)
{
	// Disable UART interrupts (transmit only):
	ETS_UART_INTR_DISABLE();

	// Clear pending interrupts:
	WRITE_PERI_REG(UART_INT_CLR(UART0), 0xFFFF);

	// Enable Tx pin:
	PIN_PULLUP_DIS(PERIPHS_IO_MUX_U0TXD_U);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, FUNC_U0TXD);

	// Set baudrate:
	uart_div_modify(UART0, UART_CLK_FREQ / BAUDRATE);

	// Configure 8N1 mode:
	WRITE_PERI_REG(UART_CONF0(UART0), 0x03 << UART_BIT_NUM_S);

	// Reset Tx FIFO:
	SET_PERI_REG_MASK(UART_CONF0(UART0), UART_TXFIFO_RST);
	CLEAR_PERI_REG_MASK(UART_CONF0(UART0), UART_TXFIFO_RST);

	// Redirect system output to uart:
	os_install_putc1(uart_putc);
}
