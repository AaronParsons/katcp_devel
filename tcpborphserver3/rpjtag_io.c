#include "rpjtag_io.h"

/*
 * This version removes the /dev/mem and direct mmap approach
 * and instead uses the lgpio library to toggle GPIO lines.
 *
 * The definitions below assume the same JTAG pins as in the original:
 *   JTAG_TMS (BCM 27)
 *   JTAG_TDI (BCM 22)
 *   JTAG_TDO (BCM 23)
 *   JTAG_TCK (BCM 24)
 *
 * The macros WAIT or debug logic remain the same, but direct register
 * macros like GPIO_SET, GPIO_CLR, etc. are replaced by calls to lgpio.
 */

/* Perspective:
 *   TMS: output from Pi -> device
 *   TDI: output from Pi -> device
 *   TDO: input  to  Pi <- device
 *   TCK: output from Pi -> device
 *
 * We'll store line handles for each line separately.
 */

static int chip = -1;          // Handle for the gpiochip (e.g. /dev/gpiochip0)
static int h_tck = -1;         // Line handle for TCK
static int h_tms = -1;         // Line handle for TMS
static int h_tdi = -1;         // Line handle for TDI
static int h_tdo = -1;         // Line handle for TDO

// We'll track TDI state in a global, just like original code
static int tdi_state = -1;

/* 
 * The old code used macros like:
 *   GPIO_SET(JTAG_TCK) 
 *   GPIO_CLR(JTAG_TCK)
 * We'll define tiny inline wrappers that do the same via lgpio.
 */

static inline void gpio_set_line(int line_handle)
{
    lgGpioWrite(line_handle, 1);
}

static inline void gpio_clr_line(int line_handle)
{
    lgGpioWrite(line_handle, 0);
}

static inline int gpio_read_line(int line_handle)
{
    return lgGpioRead(line_handle);
}

/*
 * The original code had tick_clk(), which toggles TCK high then low.
 * We'll keep that logic, but now just call our lgpio helpers.
 */
void tick_clk()
{
    gpio_set_line(h_tck);
    //nop_sleep(WAIT); // if needed, can throttle JTAG for debugging
    gpio_clr_line(h_tck);
}

/*
 * Our replacement for setup_io() which used /dev/mem + mmap.
 * Now we open the gpiochip, claim lines, etc.
 */
int setup_io()
{
    // Usually the default chip is 0 = "/dev/gpiochip0"
    chip = lgGpiochipOpen(0);
    if (chip < 0) {
        fprintf(stderr, "ERROR: lgGpiochipOpen(0) failed, rc=%d\n", chip);
        return -1;
    }

    // Claim JTAG_TCK as output
    h_tck = lgGpioClaimOutput(chip, 0, JTAG_TCK, LG_LOW);
    if (h_tck < 0) {
        fprintf(stderr, "ERROR: claim line TCK=%d failed\n", JTAG_TCK);
        goto fail;
    }

    // Claim JTAG_TMS as output
    h_tms = lgGpioClaimOutput(chip, 0, JTAG_TMS, LG_LOW);
    if (h_tms < 0) {
        fprintf(stderr, "ERROR: claim line TMS=%d failed\n", JTAG_TMS);
        goto fail;
    }

    // Claim JTAG_TDI as output
    h_tdi = lgGpioClaimOutput(chip, 0, JTAG_TDI, LG_LOW);
    if (h_tdi < 0) {
        fprintf(stderr, "ERROR: claim line TDI=%d failed\n", JTAG_TDI);
        goto fail;
    }

    // Claim JTAG_TDO as input
    h_tdo = lgGpioClaimInput(chip, 0, JTAG_TDO);
    if (h_tdo < 0) {
        fprintf(stderr, "ERROR: claim line TDO=%d failed\n", JTAG_TDO);
        goto fail;
    }

    // Everything claimed successfully
    tdi_state = -1;

    // Optionally do nop_sleep(WAIT) if you want a small delay
    nop_sleep(WAIT);

    return 0;

fail:
    // If any claim failed, release any lines we did claim and close chip
    if (h_tck >= 0) lgGpioFreeLine(h_tck);
    if (h_tms >= 0) lgGpioFreeLine(h_tms);
    if (h_tdi >= 0) lgGpioFreeLine(h_tdi);
    if (h_tdo >= 0) lgGpioFreeLine(h_tdo);
    if (chip >= 0)  lgGpiochipClose(chip);

    h_tck = h_tms = h_tdi = h_tdo = -1;
    chip  = -1;
    return -1;
}

/*
 * Our replacement for close_io() which used munmap().
 * Now we just free lines and close the gpiochip.
 */
void close_io()
{
    if (h_tck >= 0) {
        lgGpioFreeLine(h_tck);
        h_tck = -1;
    }
    if (h_tms >= 0) {
        lgGpioFreeLine(h_tms);
        h_tms = -1;
    }
    if (h_tdi >= 0) {
        lgGpioFreeLine(h_tdi);
        h_tdi = -1;
    }
    if (h_tdo >= 0) {
        lgGpioFreeLine(h_tdo);
        h_tdo = -1;
    }
    if (chip >= 0) {
        lgGpiochipClose(chip);
        chip = -1;
    }
}

/*
 * read_jtag_tdo() in the old code would do:
 *   return (GPIO_READ(JTAG_TDO)) ? 1 : 0;
 */
int read_jtag_tdo()
{
    // Just read the TDO line
    int val = gpio_read_line(h_tdo);
    return (val) ? 1 : 0;
}

/*
 * send_cmd_no_tms: sets TDI then toggles TCK (like original).
 */
void send_cmd_no_tms(int iTDI)
{
    // Set TDI if it changed
    if (iTDI == 0) {
        if (tdi_state != 0) {
            gpio_clr_line(h_tdi);
            tdi_state = 0;
        }
    } else {
        if (tdi_state != 1) {
            gpio_set_line(h_tdi);
            tdi_state = 1;
        }
    }

    // Now toggle TCK
    gpio_set_line(h_tck);
    gpio_clr_line(h_tck);
}

/*
 * The old code's send_cmd(...) sets TDI, TMS, then calls tick_clk().
 */
void send_cmd(int iTDI, int iTMS)
{
    // TDI
    if (iTDI == 1) {
        gpio_set_line(h_tdi);
        tdi_state = 1;
    } else {
        gpio_clr_line(h_tdi);
        tdi_state = 0;
    }

    // TMS
    if (iTMS == 1) {
        gpio_set_line(h_tms);
    } else {
        gpio_clr_line(h_tms);
    }

    // Then do one clock cycle
    tick_clk();
}

/*
 * reset_clk() can simply clear the TCK line
 */
void reset_clk()
{
    fprintf(stderr, "resetting clock at pin %d\n", JTAG_TCK);
    gpio_clr_line(h_tck);
    fprintf(stderr, "done\n");
}


/*
 * send_cmdWord_msb_first(...) / send_cmdWord_msb_last(...)
 * remain the same conceptually, but use send_cmd(...) for toggling lines.
 */
//Mainly used for command words (CFG_IN)
//Send data, example 0xFFFF,0,20 would send 20 1's, with not TMS
void send_cmdWord_msb_first(unsigned int cmd, int lastBit, int bitoffset)
{
    while (bitoffset--) {
        int x = (cmd >> bitoffset) & 0x01;
        send_cmd(x, (lastBit == 1 && bitoffset == 0));
    }
}


//Mainly used for IR Register codes
//Send data, example 0xFFFF,0,20 would send 20 1's, with not TMS
void send_cmdWord_msb_last(unsigned int cmd, int lastBit, int bitoffset)
{
    int i;
    for (i = 0; i < bitoffset; i++) {
        int x = (cmd >> i) & 0x01;
        send_cmd(x, (lastBit == 1 && (bitoffset == i+1)));
    }
}

/*
 * send_byte(...) and send_byte_no_tms(...) also remain,
 * but call send_cmd(...) or send_cmd_no_tms(...) for toggles.
 * Send single byte, example from fgetc
 */
void send_byte(unsigned char byte, int lastbyte)
{
    int x;
    for (x = 7; x >= 0; x--) {
        send_cmd((byte >> x) & 0x01, ((x == 0) && (lastbyte == 1)));
    }
}

void send_byte_no_tms(unsigned char byte)
{
    send_cmd_no_tms(byte & 0x80);
    send_cmd_no_tms(byte & 0x40);
    send_cmd_no_tms(byte & 0x20);
    send_cmd_no_tms(byte & 0x10);
    send_cmd_no_tms(byte & 0x08);
    send_cmd_no_tms(byte & 0x04);
    send_cmd_no_tms(byte & 0x02);
    send_cmd_no_tms(byte & 0x01);
}

/*
 * nop_sleep(...) still just does CPU nops
 * Does a NOP call in BCM2708, and is meant to be run @ 750 MHz
 */
void nop_sleep(long x)
{
    while (x--) {
        asm("nop");
    }
}

/*
 * jtag_read_data(...) remains functionally the same,
 * uses read_jtag_tdo() and send_cmd().
 */
void jtag_read_data(char* data, int iSize)
{
    if (iSize == 0) return;
    int i, temp;
    memset(data, 0, (iSize + 7) / 8);

    for (i = iSize - 1; i > 0; i--) {
        temp = read_jtag_tdo();
        send_cmd(0, 0);
        data[i / 8] |= (temp << (i & 7));
    }

    temp = read_jtag_tdo(); // read last bit while also going to EXIT
    send_cmd(0, 1);
    data[0] |= temp;
}
