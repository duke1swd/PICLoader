/*
 *  a  Load Configuration
 *  b  Load Data for Program Memory
 *  c  Load Data for Data Memory
 *  d  Read Data from Program Memory
 *  e  Read Data from Data Memory
 *  f  Increment Address
 *  g  Reset Address
 *  h  Begin Programming
 *  k  Bulk Erase Program Memory
 *  l  Bulk Erase Data Memory
 */

#define	LoadConfiguration		0
#define	LoadDataforProgramMemory	1
#define	LoadDataforDataMemory		2
#define	ReadDatafromProgramMemory	3
#define	ReadDatafromDataMemory		4
#define	IncrementAddress		5
#define	ResetAddress			6
#define	BeginProgramming		7
#define	BulkEraseProgramMemory		10
#define	BulkEraseDataMemory		11

#ifdef DEFINE_COMMANDS

static unsigned char PICcommands[] = {
	0x00,
	0x02,
	0x03,
	0x04,
	0x05,
	0x06,
	0x16,
	0x08,
	0x18,
	0x0a,
	0x09,
	0x0b,
	0x11,
};

#endif
