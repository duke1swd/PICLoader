/*
 * Simple program, mostly used to test the ICSP setup.
 *
 * Assumes an LED is on pin 7 (RA0).
 */
#pragma chip PIC12F1822
	/* 
	   This next pragma is
	   	0 Fail Save Clock Monitor Disabled.
		0 Internal / External Switchover Disabled.

		0 CLKOUT function enabled.
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
#pragma config = 0x01e4	

	/*
	   This next pragma is

	   	1 Low voltage programming enabled
		1 in circuit debugging disabled

		1 Not Implemented
		1 BORV
		1 Stack errors reset chip
		0 4x PLL under SW control

		1 Not Implemented
		1 Not Implemented
		1 Not Implemented
		1 Not Implemented, must be 1.

		1 Not Implemented
		1 Not Implemented
		1
		1 Flash memory write protect is off
	 */
#pragma config reg2 = 0x3eff


	/*
 	    Oscillator configuration register:

		1 SPLLEN -- enable 4x clock PLL
		1
		1
		1

		0 ICRF = 8 / 32 MHz.
		0 unimplemented
		0
		0 SCS = 0: Select internal clock by config register
	 */

#define	OSCCON_VALUE	0xf0




bit led @ PORTA.0;
#define	TRISMASK 0	// everything is an output

void
delay()
{
	unsigned i, j;

	for (i = 1; i; i++)
		for (j = 1; j; j++)
			nop();
}

void
main()
{
	PORTA = 0;		// Clear the port and port latch.
	LATA = 0;
	ANSELA = 0;		// All pins are digital.
	TRISA = TRISMASK;	// All ports are output.
	OSCCON = OSCCON_VALUE;	// Setup for 32 MHz operation.

	for (;;) {
		led = 1;
		delay();
		led = 0;
		delay();
	}
}
