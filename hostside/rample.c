/*
 * Very Simple program, mostly used to test the ICSP setup.
 *
 * Assumes an LED is on pin 7 (RA0).
 */
#pragma chip PIC12F1822
	/* 
	   This next pragma is
	   	0 Fail Save Clock Monitor Disabled.
		0 Internal / External Switchover Disabled.

		1 CLKOUT function disabled.
		0
		0 Brown Out Monitor disabled
		1 Data memory code protection disabled

		1 Code memory code protection disabled
		1 MCLR enabled (actually, this is ignored under LVP)
		1 Power up timer disabled
		0 

		0 Watch dog timer disabled
		1
		0
		0 FOSC=4: Internal OSC.
	 */
//#pragma config = 0x09e4
#pragma config = 0x01e4		// enable clock out function.

	/*
	   This next pragma is

	   	1 Low voltage programming enabled
		1 in circuit debugging disabled

		1 Not Implemented
		1 BORV
		1 Stack errors reset chip
		0 4x PLL disabled

		1 Not Implemented
		1 Not Implemented
		1 Not Implemented
		1 Not Implemented, must be 1.

		1 Not Implemented
		1 Not Implemented
		1
		1 Flash memory write protect is off
	 */
#pragma config reg2 = 0x3eff // not bothering with this as this is the default.


	/*
 	    Oscillator configuration register:

		1 SPLLEN -- ignored because PLL enabled on config
		1
		1
		1

		0 ICRF = 8 / 32 MHz.
		0 unimplemented
		0
		0 SCS = 0: Select internal clock by config register
	 */

//#define	OSCCON_VALUE	0xf0
#define	OSCCON_VALUE	0x10 // 31.25 KHz


bit led @ PORTA.0;
#define	TRISMASK 0	// everything is an output

void
main()
{
	//OSCCON = OSCCON_VALUE;
	PORTA = 0;		// Clear the port and port latch.
	LATA = 0;
	ANSELA = 0;		// All pins are digital.
	TRISA = TRISMASK;	// All ports are output.

	//led = 1;
	PORTA = 0;
	for (;;) nop();
}
