/*
 * This program reads a binary file
 * and uses the PICLoader program on the Arduino
 * to load it into a PIC chip.
 *
 * Built and tested ona PIC 12F1822
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>
#include "commands.h"

char *myname;
char *portbasename = "/dev/ttyS";
char *portname;
FILE *input;		// input data is here.
int fd;			// arduino is here.

int erase_mode;
#define	ERASE_NOT_SET	0
#define	ERASE_NOT	1
#define	ERASE_AND_LOAD	2
#define	ERASE_ONLY	3

int verbose;
int verify;
#define	PRINT_CONFIG	0x1
#define	PRINT_PROGRAM	0x2
#define	PRINT_DATA	0x4
int print;
int run;

#define	PIC_NUMBER_OF_LATCHES	16

static void
set_baud()
{
	int r;
	struct termios tdata;

	r = tcgetattr(fd, &tdata);
	if (r != 0) {
		fprintf(stderr, "%s: failed to get tty attrs\n",
			myname);
		perror("tcgetattr");
		exit(1);
	}

	if (verbose) {
		printf("Setting baud rate to 9600\n");
		printf("B9600 = %d, Speed = %d\n", B9600, cfgetispeed(&tdata));
	}

	r = cfsetispeed(&tdata, B9600);
	if (r != 0) {
		fprintf(stderr, "%s: failed to set tty ispeed\n",
			myname);
		exit(1);
	}

	r = cfsetospeed(&tdata, B9600);
	if (r != 0) {
		fprintf(stderr, "%s: failed to set tty ospeed\n",
			myname);
		exit(1);
	}

	r = tcsetattr(fd, TCSANOW, &tdata);
	if (r != 0) {
		fprintf(stderr, "%s: failed to set tty attrs\n",
			myname);
		exit(1);
	}

	/*
	 * Setting the baud rate resets the arduino. Why, I don't know.
	 * This sleep waits for the arduino to come out of reset.
	 */
	sleep(3);
}


static char ignore_chars[] =
	{ ' ', '\t', '\r', '\n', };
static int
arduino_read()
{
	char c;
	int r;
	int i;

	for (;;) {
		r = read(fd, &c, 1);
		if (r < 1) {
			fprintf(stderr, "%s: error reading arduino.\n",
				myname);
			exit(1);
		}

		for (i = 0; i < sizeof ignore_chars / sizeof ignore_chars[0];
		    i++) {
		    	if (c == ignore_chars[i])
				goto contin;
		}
		return c;
	    contin:;
	}
}

/* returns true if it is able to handshake with the arduino. */
static int
handshake()
{
	int c;

	write(fd, "A", 1);
	c = arduino_read();

	if (c != 'B')
		return 0;
	write(fd, "I", 1);

	if (verbose)
		printf("Handshake complete\n");
	return 1;
}

/*
 * Enters programming mode on the PIC
 */
static void
enter_program_mode()
{
	int c;

	write(fd, "E", 1);

	c = arduino_read();
	if (c != 'Y') {
		fprintf(stderr, "%s: failed to enter programming mode.\n",
			myname);
		exit(1);
	}

	if (verbose)
		printf("Now in programming mode\n");
}

static char *
gen_name(char *name)
{
	static char lbuf[128];

	sprintf(lbuf, "%s%s", portbasename, name);
	return lbuf;
}

void
l_opentty(char *name)
{
	fd = open(name, O_RDWR);
	if (fd < 0)
		return;

	if (!isatty(fd)) {
/*xxx*/printf("not a tty\n");
		close(fd);
		return;
	}
	set_baud();
}

void
openport(char *portname)
{
	int i;
	char lbuf[16];

	if (portname) {
		l_opentty(portname);
		if (fd > 0) {
			if (handshake())
				return;
			close(fd);
		}
		l_opentty(gen_name(portname));
		if (fd > 0) {
			if (handshake())
				return;
			close(fd);
		}
		fprintf(stderr, "%s: cannot open port %s or %s\n",
			myname,
			portname,
			gen_name(portname));
		exit(1);
	}

	for (i = 0; i < 32; i++) {
		sprintf(lbuf, "%d", i);
		l_opentty(gen_name(lbuf));
		if (fd > 0) {
			if (handshake())
				return;
			close(fd);
		}
	}
	fprintf(stderr, "%s: cannot find arduino\n", myname);
	exit(1);
}

/*
 * Tell the arduino to do something to the PIC
 */

#define	NO_DATA		0
#define	SEND_DATA	1
#define	READ_DATA	2

static int
send_command(int command, int data)
{
	int type;
	int len;
	int r;
	int c;
	int rdata;
	unsigned char lbuf[128];

	switch(command) {
	    case LoadConfiguration:
	    case LoadDataforProgramMemory:
	    case LoadDataforDataMemory:
	    	type = SEND_DATA;
		if (verbose)
			printf("Sending command %d with data %x\n",
				command, data);
		break;
	    case ReadDatafromProgramMemory:
	    case ReadDatafromDataMemory:
	    	type = READ_DATA;
		if (verbose)
			printf("Sending command %d.  Expecting data.\n",
				command);
		break;
	    case IncrementAddress:
	    case ResetAddress:
	    case BeginProgramming:
	    case BulkEraseProgramMemory:
	    case BulkEraseDataMemory:
	    	type = NO_DATA;
		if (verbose)
			printf("Sending command %d.\n", command);
		break;
	    default:
	    	fprintf(stderr, "%s: send command unknown %d\n",
			myname, command);
		exit(1);
	}

	lbuf[0] = command + 'a';
	len = 1;
	if (type == SEND_DATA) {
		sprintf(lbuf + 1, "%04x", data);
		len = 5;
	}
	r = write(fd, lbuf, len);

	if (r != len) {
		fprintf(stderr, "%s send command write failed %d\n",
			myname, r);
		perror("write");
		exit(1);
	}

	/*
	 * Read the return;
	 */
	rdata = 0;
	if (type == READ_DATA) {
		lbuf[0] = arduino_read();
		lbuf[1] = arduino_read();
		lbuf[2] = arduino_read();
		lbuf[3] = arduino_read();
		lbuf[4] = '\0';

		r = sscanf(lbuf, "%04x", &rdata);
		if (r != 1) {
			fprintf(stderr, "%s: scanf returned %d from: .%s.\n",
				myname, r, lbuf);
			exit(1);
		}

		if (verbose)
			printf("\tReturning data %04x\n", rdata);
	}

	c = arduino_read();

	if (c != '!') {
		fprintf(stderr, "%s: expected \'!\', got: .%c.\n",
			myname, c);
		exit(1);
	}
	return rdata;
}

/*
 * This routine can print program or config space.
 */
static void
do_print1()
{
	int i;
	int data;

	if (print & PRINT_CONFIG) {
		send_command(LoadConfiguration, 0);
		printf("\nPrinting Configuration Registers\n");
	} else {
		printf("\nPrinting first 11 words of program memmory.\n");
		send_command(ResetAddress, 0);
	}

	for (i = 0; i < 11; i++) {
		data = send_command(ReadDatafromProgramMemory, 0);
		send_command(IncrementAddress, 0);
		printf("\t%04x  %04x\n", i, data);
	}
}

/*
 * This routine can print data space
 */
static void
do_print2()
{
	int i;
	int data;

	printf("\nPrinting first 10 words of Data Memory\n");
	send_command(ResetAddress, 0);
	for (i = 0; i < 10; i++) {
		data = send_command(ReadDatafromDataMemory, 0);
		send_command(IncrementAddress, 0);
		data &= 0xff;
		printf("\t  %02x    %02x\n", i, data);
	}
}

/* The next word we receive goes here. */
int pic_address;

/* number of words in the programming latches. */
int data_in_latches;

static void
flush_latches()
{
	if (!verify && data_in_latches > 0)
		send_command(BeginProgramming, 0);
	data_in_latches = 0;
}


/*
 * Start programming the PIC.
 *
 * The Arduino is on fd, the data comes in on FILE input.
 */
static void
doit()
{
	int i;
	int r;
	int len;
	int data;
	int vdata;
	int lineno;
	int config;
	int pic_number_of_latches;
	char lbuf[128];

	pic_address = 0;
	lineno = 0;
	config = 0;
	data_in_latches = 0;
	pic_number_of_latches = PIC_NUMBER_OF_LATCHES;

	/*
	 * Loop over lines of input.
	 */
	while(fgets(lbuf, sizeof lbuf, input) == lbuf) {
		len = strlen(lbuf) - 1;
		lineno++;

		if (verbose)
			printf("INPUT (%3d): %s", lineno, lbuf);

		r = sscanf(lbuf + 1, "%04x", &data);
		if (r != 1) {
			fprintf(stderr, "%s: scanf returned %d "
					"from: .%s.\n",
				myname, r, lbuf);
			exit(1);
		}

		switch(lbuf[0]) {
		    case 'C':
		    	
			/* Enter config space programming mode */
			pic_number_of_latches = 1;
			config = 1;
			flush_latches();
			pic_address = 0;
			if (verify)
				fprintf(stderr, "%s: Warning: cannot "
						"verify config space.\n",
					myname);
			break;

		    case 'A':
		    	if (config != 1) {
				fprintf(stderr, "%s: ilegal A record\n",
					myname);
				exit(1);
			}
			config = 2;
			flush_latches();
			pic_address = data;
			break;
		
		/*
		 * NOTE: this doesn't work.  Why???   *************
		 */
		    case 'S':
			flush_latches();
			pic_address += data;
			if (verbose)
				printf("skipping %d words\n", data);
			while (data-- > 0)
				send_command(IncrementAddress, 0);
			break;
		
		    case 'P':
			/* A word for program memory */
			if (verify && config == 0) {
				/* this code doesn't handle configuration */
				vdata =
				    send_command(ReadDatafromProgramMemory, 0);
				if (vdata != data) {
					fprintf(stderr, "%s: verify error on "
							"line %d\n",
						myname,
						lineno);
					if (verbose) {
						fprintf(stderr, "\tExpected %x ",
							data);
						fprintf(stderr, "\t-- Got %x ",
							vdata);
					}
					exit(1);
				}
			} else if (!verify) {
				if (config == 2 || config == 1) {
					send_command(LoadConfiguration, data);
					for (i = 0; i < pic_address; i++)
						send_command(IncrementAddress,
							0);
					config = 3;
				} else
					send_command(LoadDataforProgramMemory,
						data);
				data_in_latches++;
				if (data_in_latches >= pic_number_of_latches)
					flush_latches();
			}
			send_command(IncrementAddress, 0);
			pic_address++;
			break;

		    default:
		    	fprintf(stderr, "%s: cannot understand input "
					"command %c\n",
				myname, lbuf[0]);
			exit(1);
		}
	}
	flush_latches();
}

/*
 * Erase the PIC
 */
static void
erase()
{
	if (verbose)
		printf("*** Erasing\n");
	send_command(BulkEraseProgramMemory, 0);
	send_command(BulkEraseDataMemory, 0);
}

/*
 * Done with programing.
 */
static void
done()
{
	int c;

	write(fd, "x", 1);

	c = arduino_read();

	if (c != '!') {
		fprintf(stderr, "%s: expected \'!\', got: .%c.\n",
			myname, c);
		exit(1);
	}
}

static void
post()
{
	if (run) {
		write(fd, "R", 1);
		sleep(2);
		enter_program_mode();
		do_print2();
	}
	write(fd, "Z", 1);
}

static void
set_defaults()
{
	portname = NULL;
	erase_mode = ERASE_NOT_SET;
	verbose = 0;
	verify = 0;
	print = 0;
	run = 0;
}

static void
usage()
{
	set_defaults();

	fprintf(stderr, "Usage: %s <options> [<hexfile>]\n", myname);
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "\t-p <com port>\n");
	fprintf(stderr, "\t-e (do NOT erase before loading)\n");
	fprintf(stderr, "\t-E (erase only, no loading)\n");
	fprintf(stderr, "\t-V (verify only, no erase, no programming)\n");
	fprintf(stderr, "\t-v (verbose mode)\n");
	fprintf(stderr, "\t-C (print out config space)\n");
	fprintf(stderr, "\t-P (print out a bit program space)\n");
	fprintf(stderr, "\t-D (print out a bit data space)\n");
	fprintf(stderr, "\t-r (run program, wait 2 seconds, print data)\n");
	exit(1);
}

static void
grok_args(int argc, char **argv)
{
	int c;
	int nargs;
	int errors;
	extern char *optarg;
	extern int optind;

	myname = argv[0];
	errors = 0;

	while ((c = getopt(argc, argv, "rDPCVeEp:vh")) != EOF)
	switch (c) {

	    case 'r':
	    	run++;
		break;

	    case 'D':
	    	print |= PRINT_DATA;
		break;

	    case 'P':
	    	print |= PRINT_PROGRAM;
		break;

	    case 'C':
	    	print |= PRINT_CONFIG;
		break;

	    case 'V':
	    	verify++;
		break;

	    case 'e':
	    	erase_mode = ERASE_NOT;
		break;

	    case 'E':
	    	erase_mode = ERASE_ONLY;
		break;

	    case 'p':
	    	portname = optarg;
		break;

	    case 'v':
	    	verbose++;
		break;

	    case 'h':
	    case '?':
	    default:
		usage();
	}

	if (print && erase_mode != ERASE_NOT_SET) {
		fprintf(stderr, "%s: -D/-P/-C not compatible with -e or -E\n",
			myname);
		errors++;
	}

	if (erase_mode == ERASE_NOT_SET) {
		if (print || verify)
			erase_mode = ERASE_NOT;
		else
			erase_mode = ERASE_AND_LOAD;
	} else if (verify) {
		fprintf(stderr, "%s: cannot erase and verify in same pass\n",
			myname);
		errors++;
	}

	if (print && verify) {
		fprintf(stderr, "%s: only one of -D/-P/-C and -V permitted.\n",
			myname);
		errors++;
	}

	nargs = argc - optind;

	if (nargs > 0 && print) {
		fprintf(stderr, "%s: no opt args when -D/-P/-C\n", myname);
		errors++;
	}

	if (nargs > 1)
		errors++;
	else if (nargs == 1) {
		if (erase_mode == ERASE_ONLY)
			fprintf(stderr, "%s: WARNING file name %s "
					"ignored in erase only mode.\n",
				myname, argv[optind]);

		input = fopen(argv[optind], "r");
		if (input == NULL) {
			fprintf(stderr, "%s: cannot open "
					"%s for reading.\n",
				myname,
				argv[optind]);
			errors++;
		}
	} else
		input = stdin;

	if (errors)
		usage();
}

int
main(int argc, char **argv)
{
	grok_args(argc, argv);

	openport(portname);

	enter_program_mode();

	if (erase_mode != ERASE_NOT)
		erase();

	if (print) {
		if (print & PRINT_CONFIG)
			do_print1();
		print &= ~PRINT_CONFIG;
		if (print & PRINT_PROGRAM)
			do_print1();
		if (print & PRINT_DATA)
			do_print2();
	} else if (erase_mode != ERASE_ONLY)
		doit();

	done();
	post();

	exit(0);
}
