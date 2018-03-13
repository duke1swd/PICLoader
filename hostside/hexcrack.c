/*
 * Program to read and interpret the hex output of cc5x
 *
 * Based on http://www.lucidtechnologies.info/inhx32.htm
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *myname;
int verbose;

char buffer[128];

int lineno;
int sum;
int lastline;
int current_address;
// 0 = normal, 1 = entering config, not yet decided what type 2 = fixed.
int config_space;

int
hexdigit(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;;

	fprintf(stderr, "%s: line %d contains invalid hex digit %c\n",
		myname, lineno, c);
	exit(1);
}

int
hexbyte(char *p)
{
	int t;

	t =  hexdigit(p[0]) * 16 + hexdigit(p[1]);
	sum += t;
	return t;
}

void
process()
{
	int i;
	int type;
	int count;
	int len;
	int nbytes;
	int address;
	int extended_address;
	int checksum;
	unsigned char bytes[128];
	char *p;

	sum = 0;

	if (lastline) {
		fprintf(stderr, "%s lineno %d occurs after last line "
				"marker.\n",
			myname,
			lineno);
		exit(1);
	}

	if (buffer[0] != ':') {
		fprintf(stderr, "%s: line %d doesn't begin with a ':'\n",
			myname, lineno);
		exit(1);
	}

	len = strlen(buffer);
	while (buffer[len-1] == '\n' || buffer[len-1]== '\r')
		len--;

	if (len < 11) {
		fprintf(stderr, "%s: line %d too short (%d)\n", 
			myname, lineno, len);
		exit(1);
	}

	nbytes = (len - 11) / 2;

	count = hexbyte(buffer + 1);

	if (len != 11 + 2 * count) {
		fprintf(stderr, "%s: line %d wrong length.  (%d %d)\n",
			myname,
			lineno,
			len,
			11 + 2 * count);
		exit(1);
	}

	address = 0x100 * hexbyte(buffer + 3) + hexbyte(buffer + 5);

	type = hexbyte(buffer+7);

	p = buffer + 9;
	for (i = 0; i < nbytes; i++)
		bytes[i] = hexbyte(p + i * 2);

	checksum = sum;
	sum = 0;

	if ((0xff & (-1 * checksum)) != hexbyte(buffer + len - 2)) {
		fprintf(stderr, "%s: line %d checksum error.  %02x  %02x\n",
			myname,
			lineno,
			0xff & (-1 * checksum),
			sum);
		exit(1);
	}


	switch(type) {
	    case 0:
	    	if (config_space) {
		    if (address < current_address) {
			fprintf(stderr, "%s: line no %d config load address "
					"skips backwards from %x to %x\n",
				myname,
				lineno,
				current_address,
				address);
			exit(1);
		    } else if (address != current_address) {
			printf("A%04x\n", address/2);
			current_address = address;
		    }
		} else {
		    if (address != current_address) {
#ifdef notworking
		    	printf("S%04x\n", (address - current_address)/2);
#else
			for (i = 0; i < (address - current_address)/2; i++)
				printf("P0000\n");
#endif
			current_address = address;
		    }
		}

		for (i = 0; i < nbytes; i+= 2) {
			printf("P%02x%02x\n",
				bytes[i+1],
				bytes[i]);
			current_address += 2;
		}

	    	break;

	    case 1:
	    	if (address != 0 || nbytes != 0) {
			fprintf(stderr, "%s: line no %d "
					"unknown type 1 record\n", 
				myname,
				lineno);
			fprintf(stderr, "\taddress = %x, nbytes = %d\n",
				address,
				nbytes);
			exit(1);
		}
		lastline++;
	    	break;

	    case 4:
	    	if (address != 0 || nbytes != 2) {
			fprintf(stderr, "%s: line no %d "
					"unknown type 4 record\n", 
				myname,
				lineno);
			fprintf(stderr, "\taddress = %x, nbytes = %d\n",
				address,
				nbytes);
			exit(1);
		}

		extended_address = (bytes[0] << 8) | bytes[1];
		if (config_space) {
			// already in config space
			if (extended_address == 0) {
			    fprintf(stderr, "%s: line no %d "
					"exit from config space NYI\n",
				myname, lineno);
			    exit(1);
			} else if (current_address != 0) {
			    fprintf(stderr, "%s: line no %d "
					"reset within config space NYI\n",
				myname, lineno);
			    exit(1);
			}
		} else {
			// not currently in config space
			if (extended_address == 1) {
				// entering config space
				config_space = 1;
				current_address = 0;
				printf("C0000\n");
			} else if (current_address != 0) {
				fprintf(stderr, "%s: line no %d "
						"general type 4 records NYI\n", 
				    myname,
				    lineno);
				fprintf(stderr, "\tExtended address = %04x\n",
					extended_address);
				exit(1);
			}
		}
	    	break;

	    default:
		fprintf(stderr, "%s: line %d invalid type code %d\n",
			myname, lineno, type);
		exit(1);
	}

	if (verbose) {
		printf("%2d %4x %d ", len, address, type);
		for(i = 0; i < nbytes; i++)
			printf(" %02x", bytes[i]);
		printf("\n");
	}
}


int
main(int argc, char **argv)
{
	FILE *input;

	myname = *argv;
	verbose = 0;

	lineno = 1;
	lastline = 0;
	input = stdin;
	current_address = 0;
	config_space = 0;
	while (fgets(buffer, sizeof buffer, input) == buffer) {
		process();
		lineno++;
	}
	if (!lastline) {
		fprintf(stderr, "%s: no type 1 record found\n", myname);
		exit(1);
	}
	return 0;
}
