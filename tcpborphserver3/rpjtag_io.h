#ifndef RPJTAG_IO_H
#define RPJTAG_IO_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <lgpio.h>

//Perspective is from Device connected, so TDO is output from device to input into rpi
#define JTAG_TMS 27 //PI ---> JTAG
#define JTAG_TDI 22 //PI ---> JTAG
#define JTAG_TDO 23 //PI <--- JTAG
#define JTAG_TCK 24 //PI ---> JTAG

//-D DEBUG when compiling, will make all sleeps last 0.5 second, this can be used to test with LED on ports, or pushbuttons
//Else sleeps can be reduced to increase speed
#ifdef DEBUG
#define WAIT 10000000 // approx 0.5s
#else
#define WAIT 1000 // approx 0.5us
#endif

int setup_io();
void close_io();
void reset_clk();
int read_jtag_tdo();
void send_cmd_no_tms(int iTDI);
void send_cmd(int iTDI,int iTMS);
void send_cmdWord_msb_first(unsigned int cmd, int lastBit, int bitoffset);
void send_cmdWord_msb_last(unsigned int cmd, int lastBit, int bitoffset);
void send_byte(unsigned char byte, int lastbyte);
void send_byte_no_tms(unsigned char byte);
void nop_sleep(long x);
void jtag_read_data(char* data,int iSize);

#endif
