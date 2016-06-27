/* topgen.c - parse a topology specification and build an initial	*/
/*		database or update an existing database			*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/************************************************************************/
/*									*/
/* use:   topgen  [-s] other_args					*/
/*									*/
/*	where other_args are either					*/
/*									*/
/*		-u topname						*/
/*	or								*/
/*		file							*/
/*									*/
/* Topgen performs one of two actions.  With argument -u and a topology	*/
/*   name, topgen uses topname to find an existing topology database	*/
/*   file, applies changes (additions and deletions of connectivity),	*/
/*   and creates one or more updated topology database files, as	*/
/*   requested.	 Given a file name, topgen assumes the file contains	*/
/*   a topology specification, parses the specification, and creates an	*/
/*   initial topology database in a file with a .0 suffix (e.g., if the	*/
/*   specifcation file is named X, the database will be X.0).		*/
/*   The optional -s flag makes all communication symmetric, which	*/
/*   means that if the input specifies node A can hear node B, topgen	*/
/*   automatically infers that node B can also hear node A.  Similarly	*/
/*   if node A cannot hear node B, topgen infers that B cannot hear A.	*/
/*									*/
/* An initial topology specification file is a text file. A pound sign	*/
/*   starts a comment, and the rest of the line is ignored.  Arbitrary	*/
/*   whitespace	(blanks, tabs, and newlines) is allowed among items.	*/
/*   The file consists of a series of entries, where each entry lists	*/
/*   a sending node name terminated by a colon followed by zero or more	*/
/*   receiving node names.  For example,				*/
/*									*/
/*	a: b c x3 qqq							*/
/*									*/
/*   specifies that node a can send to nodes b, c, x3, and qqq.		*/
/*   Node names can contain uppercase or lowercase letters, digits,	*/
/*   underscores and periods, but must start with a non-digit.		*/
/*									*/
/* 									*/
/* When the -u option is present, 					*/
/*	  1) On any input line, everything to the right of a pound	*/
/*		sign (#) is a comment.					*/
/*									*/
/*	  2) To specify that node a can send to node b, a user enters:	*/
/*									*/
/*		        + a b						*/
/*									*/
/*	  3) To specify that x can no longer send to y, a user enters:	*/
/*									*/
/*			- x y						*/
/*									*/
/*	  4) As a shorthand, * specifies all nodes, so to specify that	*/
/*		all nodes can receive from node q, a user enters:	*/
/*									*/
/*			+ q *						*/
/*									*/
/*		and to specify that node w cannot receive from any	*/
/*		node, a user can enter:					*/
/*									*/
/*			- * w						*/
/*									*/
/*	  5) After a series of + and - commands, a user can enter	*/
/*									*/
/*			write						*/
/*									*/
/*		to specify that a snapshot of the current topology be	*/
/*		written to a file.					*/
/*									*/
/*									*/
/*	The argument to topgen specifies a topology database, either	*/
/*	by giving a topology name or a complete topology database name.	*/
/*	A topology database has a name of the form T.N, where T is a	*/
/*	topology name and N is a numerical suffix in			*/
/*	decimal.  When	it generates the initial database for topology	*/
/*	X, the parser stores the database in a file named X.0. 		*/
/*									*/
/*									*/
/************************************************************************/


#define	NAMLEN	128		/* Maximum length of a node name	*/
#define	NODES	 46		/* Maximum number of nodes		*/

#define	TOKEOF	 -1		/* Token for End-Of-File		*/
#define	TOKRECV	  0		/* Token of the form xxx (a receiver)	*/
#define	TOKSEND	  1		/* Token of the form xxx: (a sender)	*/

#define	RECVUDEF  0		/* No specification of receivers has	*/
				/*   been encoutered for this node	*/
#define	RECVDEF   1		/* A specification of receivers has	*/
				/*   been encoutered for this node	*/

#define	CHARRNG	256		/* The integer range of a char		*/

/* Chracter definitions */

#define	BLANK	' '
#define	SHARP	'#'
#define	NEWLINE	'\n'
#define	COLON	':'
#define	TAB	'\t'
#define	NULLCH	'\0'

int	linenum = 1;		/* The line number (for error messages)	*/

int	map[CHARRNG];		/* Map of chracters allowed in a token	*/

struct	node {			/* An entry in the list of node names	*/
	int	nstatus;	/* Have we seen this node already?	*/
	int	nrecv;		/* Can the node receive from any other?	*/
	int	nsend;		/* Can the node send to any others?	*/
	char	nname[NAMLEN];	/* Name of the node			*/
	unsigned char nmcast[6];/* Multicast address to use when sending*/
};

struct	node	nodes[NODES];	/* List of node names			*/
int	nnodes = 0;		/* Current number of names in the nodes	*/
				/*	array				*/

/************************************************************************/
/*									*/
/* errexit - print an error message and exit				*/
/*									*/
/************************************************************************/

void	errexit (
	  char	*fmt,		/* Printf format to use			*/
	  long	arg1,		/* First argument to printf		*/
	  long	arg2		/* Second argument to printf		*/
	)
{
	fprintf(stderr, fmt, arg1, arg2);
	exit(1);
}

/************************************************************************/
/*									*/
/* init - initialize							*/
/*									*/
/************************************************************************/

void	init (void)
{
	int	i;		/* Array index used with nap and nodes	*/
	int	j;		/* Index in a multicast array		*/
	struct	node	*nptr;	/* Ptr to an entry in modes		*/

	/* Initialize the nodes array */

	for (i=0; i<NODES; i++) {
		nptr = &nodes[i];
		nptr->nstatus = RECVUDEF;
		nptr->nrecv = nptr->nsend = 0;
		nptr->nname[0] = NULLCH;
		nptr->nmcast[0]= 0x01;
		for (j=1; j<6; j++) {
			nptr->nmcast[j] = 0x00;
		}
	}

	/* Initialize the token map (i.e., the set of characters */
	/*	 allowed in a node name plus color)		 */

	/* Begin by disallowing all characters */

	for (i=0; i<CHARRNG; i++) {
		map[i] = 0;
	}

	/* Allow lowercase letters */

	for (i='a'; i<='z'; i++) {
		map[i] = 1;
	}

	/* Allow lowercase letters */

	for (i='A'; i<='Z'; i++) {
		map[i] = 1;
	}

	/* Allow digits */

	for (i='0'; i<='9'; i++) {
		map[i] = 1;
	}

	/* Allow underscore, period, and colon */

	map['_'] = map['.'] = map[':'] = 1;

	return;
}


/************************************************************************/
/*									*/
/* skipblanks - skip whitespace and NEWLINES, and return the next char	*/
/*									*/
/************************************************************************/

int	skipblanks(void)
{
	int	ch;		/* Next character read from stdin */

	/* Get one character */

	ch = fgetc(stdin);

	/* Skip by whitespace */

	while ((ch==BLANK) || (ch==TAB) || (ch==NEWLINE) || (ch==SHARP)) {

		/* If end-of-file has been reached, return EOF */

		if (ch == EOF) {
			return EOF;
		}

		/* Ignore # comment end-of-line (or EOF) */

		if (ch == SHARP) {
			while( (ch!=NEWLINE) && (ch!=EOF)) {
				ch = fgetc(stdin);
			}
			if (ch == EOF) {
				return EOF;
			}
		}
		if (ch == NEWLINE) {
			linenum++;
		}
		ch = fgetc(stdin);
	}

	/* Return a non-whitespace character or EOF */

	return ch;
}


/************************************************************************/
/*									*/
/* gettok - get the next token, one of TOKSEND, TOKRECV, TOKEOF		*/
/*									*/
/************************************************************************/

int	gettok(
	  char *tok		/* Buffer used to store the token	*/
	 )
{

	int	ch;		/* Next character read from stdin	*/
	int	colonseen;	/* Have we seen a colon?		*/
	int	len;		/* The number of characters in the token*/

	len = colonseen = 0;

	/* Find the first character of the token */

	ch = skipblanks();

	/* If end-of-file was encountered, return the EOF token */

	if (ch == EOF) {
		return TOKEOF;
	}

	/* Accumulate characters of the token */

	while ((ch!=BLANK) && (ch!=TAB) && (ch!=EOF) && (ch!=NEWLINE) ) {
		if (ch == COLON) {
			if ( (colonseen>0) || (len==0) ) {
				errexit("error: misplaced colon on line %d\n", linenum, 0);
			}
			colonseen = 1;
		} else if (map[ch] == 0) {
			if (isprint(ch)) {
				errexit("error: illegal character %c appears on line %d\n",ch,linenum);
			} else {
				errexit("error: unprintable character appears on line %d\n",linenum, 0);
			}
		}
		tok[len++] = ch;
		if (len >= NAMLEN - 1) {
			errexit("error: token exceeds maximum size on line %d\n", linenum, 0);
		}
		ch = fgetc(stdin);
	}

	tok[len] = NULLCH;

	/* Ensure that the first character of the token is not numeric */

	if ( (tok[0]>='0') && (tok[0]<='9') ) {
		errexit("error: on line %d, token name '%s' starts with a digit\n",linenum, (int)tok);
	}

	if (ch == NEWLINE) {
		linenum++;
	}

	if (colonseen > 0) {
		if (len == 1) {
			errexit("error: stray colon found on line %d\n", linenum, 0);
		}

		/* Remove the trailing colon */

		len--;
		tok[len] = NULLCH;
		return TOKSEND;
	}
	return TOKRECV;
}


/************************************************************************/
/*									*/
/* srbit - set or read the bit in a 48-bit Ehternet multicast address	*/
/*		that corresponds to a node ID.				*/
/*									*/
/* Arguments are:							*/
/*									*/
/*	The node ID in the range 0 through 45				*/
/*	Pointer to a multicast address (array of six bytes)		*/
/*	operation: either BIT_SET or BIT_TEST				*/
/*									*/
/* Bit assignment:							*/
/*	A multicast address is placed in an array of bytes in memory,	*/
/*	stored in big-endian byte order.  Each node ID corresponds to	*/
/*	one of the bits, starting at the least-significant bit in the	*/
/*	least-significant byte.  Thus, to set the bit for node 0,	*/
/*	we compute the logical or of mask 0x01 with address[5].  Node 1	*/
/*	uses 0x02 with address[5].  Node 7 uses mask 0x80 with		*/
/*	address[5], and node 8 uses mask 0x01 with address[4].  The	*/
/*	only exception occurs in the high-order byte (address[0])	*/
/*	because Ethernet reserves the low-order bit for multicast.	*/
/*	Therefore, the assignment of bits in high-order byte is		*/
/*	shifted one bit to the left (i.e., the first bit we use is	*/
/*	0x02.								*/
/*									*/
/************************************************************************/

#define	BIT_SET		 0	/* Command to set a bit   */
#define	BIT_TEST	 1	/* Command to test a bit  */
#define BIT_RESET	 2	/* Command to reset a bit */
/* Definitions to make code compatibile with Xinu */
#define	SYSERR		-1
typedef	unsigned char	byte;

int	srbit (
	  byte	addr[],		/* A 48-bit Ethernet multicast address	*/
	  int	nodeid,		/* A node ID (0 - 45)			*/
	  int	cmd		/* BIT_SET or BIT_TEST or BIT_RESET   	*/
	)
{
	int	aindex;		/* The byte index in the address array	*/
				/*	(0 through 5)			*/
	int	mask;		/* A byte with a 1 in the bit position	*/
				/*	to use and 0 in other positions	*/

	/* Ensure that the bit value is valid */

	if ( (nodeid < 0) || (nodeid > 45) ) {
		return SYSERR;
	}

	/* Compute a mask 2^(floor(nodeid modulo 8))*/

	mask = 1 << (nodeid % 8);

	/* Compute the appropriate array index */

	aindex = 5 - (nodeid >> 3);

	/* Adjust the mask one bit for the high-order byte */

	if (aindex == 0) {
		mask = mask << 1;
	}

	/* If command specifies setting, change set the bit */

	if (cmd == BIT_SET) {
		addr[aindex] |= mask;
		return 1;
	}

	/* If command specifies resetting, reset the bit to 0 */

	if (cmd == BIT_RESET) {
	        addr[aindex] &= (!mask);
		return 2;
	}

	/* Command specifies testing */

	if ( (addr[aindex] & mask) == 0) {
		return 0;	/* Bit is 0 */
	} else {
		return 1;	/* Bit is 1 */
	}
}



/************************************************************************/
/*									*/
/* lookup - look up a token name					*/
/*									*/
/************************************************************************/

int	lookup(
	  char	*tok		/* A token name to look up in the list	*/
	)

{
	int	i;		/* Index in the token list		*/
	struct	node	*nptr;	/* Pointer to an entry in nodes		*/

	for (i=0; i<nnodes; i++) {
		nptr = &nodes[i];
		if (strcmp(tok,nptr->nname) == 0) {
			return i;
		}
	}
	nptr = &nodes[nnodes++];
	if (nnodes >= NODES) {
		errexit("Maximum number of nodes exceeded on line %d\n",linenum, 0);
	}
	strcpy(nptr->nname, tok);
	return i;
}

int node_can_send(int nodeid) {
  int i;
  for (i = 0; i < nnodes; i++) {
    if (srbit(nodes[nodeid].nmcast, i, BIT_TEST) == 1) {
      return 1;
    }
  }
  return 0;
}

int node_can_receive(int nodeid) {
  int i;
  for (i = 0; i < nnodes; i++) {
    if (srbit(nodes[i].nmcast, nodeid, BIT_TEST) == 1)
      return 1;
  }
  return 0;
}

void set_reset_send_all(int nodeid, int bit_op) {
  int i;
  for (i = 0; i < nnodes; i++) {
    srbit(nodes[nodeid].nmcast, i, bit_op);
  }
}

void set_reset_receive_all(int nodeid, int bit_op) {
  int i;
  for (i = 0; i < nnodes; i++) {
    srbit(nodes[i].nmcast, nodeid, bit_op);
  }
}

/************************************************************************/
/*									*/
/* main program								*/
/*									*/
/************************************************************************/

int	main(
	  int	argc,
	  char	*argv[]
	)
{
	char	tok[NAMLEN];		/* The next input token		*/
	int	typ;			/* Type of a token		*/
	int	sindex;			/* Index of sender in nodes	*/
	int	rindex;			/* Index of receiver in nodes	*/
	char	*infile;		/* Ptr to input file name	*/
	char	*outfile;		/* Ptr to output file name	*/
	FILE	*fout;			/* Stdio file ptr for outfile	*/
	FILE	*fin;			/* Stdio file ptr for intop	*/
	struct	node	*sptr;		/* Ptr to sending node entry	*/
	struct	node	*rptr;		/* Ptr to receiving node entry	*/
	int	nindex;			/* Index into the nodes array	*/
	int	symmetric = 0;		/* Nonzero => force symmetry	*/
	int	i, j;			/* Indicies used for bytes and	*/
					/*  bits of a multicast address	*/
	char	*msg;			/* Used to select a message to	*/
					/*  print			*/
	unsigned char sentinel[6];	/* Sentinel value in the file	*/
	unsigned char	nlen;		/* Length of a node name	*/

	unsigned char buffer[6], namebuffer[NAMLEN];
	unsigned char name[NAMLEN];
	unsigned char length[1];
	char op[1], operand_1[NAMLEN], operand_2[NAMLEN];
	char update_string[260];
	int nodeid_1, nodeid_2;
	int plus_minus;
	struct node *nptr;

	int parse = 1;

	char	use[] = "error: use is topgen [-s] (filename | -u topname)\n";

	/* Process arguments */

	if ( (argc!=2) && (argc!=3) && (argc!= 4) ) {
		fprintf(stderr, "%s", use);
		exit(1);
	}

	if (argc == 3) {
		if (strcmp(argv[1], "-s") == 0) {
			symmetric = 1;
			argv++;
		}

		else if (strcmp(argv[1], "-u") == 0) {
			parse = 0;
			argv++;
		}

		else {
			fprintf(stderr, "%s", use);
			exit(1);
		}
	}

	if (argc == 4) {
		if (strcmp(argv[1], "-s") == 0) {
			symmetric++;
			argv++;
			if (strcmp(argv[2], "-u") == 0) {
				parse = 0;
				argv++;
			}

			else {
				 fprintf(stderr, "%s", use);
				 exit(1);
			}
		}

		else {
			fprintf(stderr, "%s", use);
			exit(1);
		}
	}


	if (parse) {
		/*Reopen stdin to be the topology file */

		infile = argv[1];
		if (freopen(infile, "r", stdin) == NULL) {
			fprintf(stderr, "error: cannot read input file %s\n", infile);
			exit(1);
		}

		/* Initialize data structures */

		init();

		/* Start with the first token */

		typ = gettok(tok);
		if (typ == TOKEOF) {
			fprintf(stderr, "error: no tokens found in input file %s\n", infile, 0);
			exit(1);
		}

		/* While items remain to be processed */

		while(1) {

			/* Verify that the next item is the name of a sending node */

			if (typ != TOKSEND) {
				errexit("Sending node expected on line %d\n", linenum, 0);
			}

			/* Lookup name of sender and verify that the sender did	*/
			/*	not have a previous specification		*/

			sindex = lookup(tok);
			sptr = &nodes[sindex];
			if (sptr->nstatus == RECVDEF) {
				errexit("error: multiple definitions for node %s on line %d\n", (long)tok, linenum);
			}

			/* Mark the sender as having appeared in a definition */

			sptr->nstatus = RECVDEF;

			/* Get the first receiver on the list */

			typ = gettok(tok);

			if (typ != TOKRECV) {
				/* The node does not have any receivers on its	*/
				/*  list.  An error exit can be inserted here	*/
				/*  isolated nodes are not allowed (the current	*/
				/*  version allows them to permit testing the	*/
				/*  effects of isolation.			*/

				if (typ == TOKEOF) {
					break;
				} else {
					continue;
				}
			}

			/* While additional receiving nodes are found, add each to the list of receivers */

			while (typ == TOKRECV) {
				rindex = lookup(tok);
				if (rindex == sindex) {
					errexit("error: node %s cannot be a receiver for itself (line %d)\n", (long)tok, linenum);
				}
				rptr = &nodes[rindex];
				rptr->nrecv = 1;
				sptr->nsend = 1;
				srbit(sptr->nmcast, rindex, BIT_SET);
				if (symmetric > 0) {
					/* Force symmetry */
					srbit(nodes[rindex].nmcast, sindex, BIT_SET);
					rptr->nsend = 1;
					sptr->nrecv = 1;
				}
				/* Move to the next token */
				typ = gettok(tok);
			}
			if (typ == TOKEOF) {
				break;
			}
		}

		/* Analyze the results */

		for (nindex=0; nindex<nnodes; nindex++) {
			sptr = &nodes[nindex];
			printf("Node %2d ", nindex);
			msg="Can send & receive";
			if (sptr->nsend == 0) {
				msg = "Can only receive";
				if (sptr->nrecv == 0) {
					msg = "Completely isolated";
				}
			} else if (sptr->nrecv == 0) {
				msg = "Can only send";
			}
			printf(" %-19s",msg);
			printf(" (original name %s)\n", sptr->nname);
			printf("         Multicast address: ");
			for (i=0;i<6;i++) {
				printf(" ");
				for (j=7; j>=0; j--) {
					printf("%d",(sptr->nmcast[i]>>j)&0x01);
				}
			}
			printf("\n");
		}

		/* Output a topology database */

		outfile = malloc(strlen(infile)+3);
		strcpy(outfile, infile);
		strcat(outfile, ".0");
		if ( (fout = fopen(outfile, "w") ) == NULL) {
			fprintf(stderr,"error - cannot open output file %s\n", outfile);
		}

		/* Write the multicast address for each node */

		for (nindex=0; nindex<nnodes; nindex++) {
			fwrite(nodes[nindex].nmcast, 1, 6, fout);
		}

		/* Write the sentinel value */

		sentinel[0] = sentinel[1] = sentinel[2] = sentinel[3] =
			sentinel[4] = sentinel[5] = 0x00;
		fwrite(sentinel, 1, 6, fout);

		/* Write the node names as a 1-byte length field followed by	*/
		/*	a set of 9null-terminated) characters that form the	*/
		/*	name of the node.  The length includes the null byte.	*/

		for (nindex=0; nindex<nnodes; nindex++) {
			nlen = (strlen(nodes[nindex].nname) + 1) & 0xff;
			fwrite(&nlen, 1, 1, fout);
			fwrite(nodes[nindex].nname, 1, nlen, fout);
		}

		fclose(fout);
	}

	/* Update branch */

	else {
		fin = fopen(argv[1], "rb");
		sentinel[0] = sentinel[1] = sentinel[2] = sentinel[3] =
			sentinel[4] = sentinel[5] = 0x00;
		printf("%s\n", argv[1]);
		while(fread(buffer, 1, sizeof(buffer), fin) > 0) {
			if(memcmp(buffer, sentinel, 6) == 0) {
				break;
			}
			nptr = &nodes[nnodes++];
			for(int j = 0; j < 6; j++) {
			  nptr -> nmcast[j] = buffer[j];
			}
		}

		nnodes = 0;
		while(fread(buffer, 1, 1, fin) > 0) {
			memcpy(length, buffer, 1);
			//sprintf(length, "%x", l);
			printf("\nlength original: %x\n", buffer[0]);
			//printf("\nlength: %d\n", l);
			fread(namebuffer, 1, (int) buffer[0], fin);
			memcpy(name, namebuffer, (int)buffer[0]);
			//print_uc(length, 1);
			nptr = &nodes[nnodes++];
			strcpy(nptr->nname, name);
			//printf("\nlength: %d\n", (int)strtol(length, NULL, 16));
			printf("\nname: %s\n", nptr->nname);
			printf("\nmulticast address: ");
			//print_uc(nptr->nmcast, 6);
		}

		printf("sentinel: %llx\n", *sentinel);
		fclose(fin);

		scanf("%[^\n]", update_string);
		getchar();
		//change "insert" to update command constant later

		while(strcmp(update_string, "insert") != 0) {
			char *update_string_temp = update_string;
			strcpy(op, strsep(&update_string_temp, " "));
			strcpy(operand_1, strsep(&update_string_temp, " "));
			strcpy(operand_2, strsep(&update_string_temp, " "));
			if(!(strcmp(op, "+") == 0 || strcmp(op, "-") == 0)) {
				printf("invalid operation");
			}

			if (strcmp(op, "+") == 0)
				plus_minus = BIT_SET;
			else
				plus_minus = BIT_RESET;

			if (strcmp(operand_1, "*") == 0) {
				if (strcmp(operand_2, "*") == 0) {
					for (i = 0; i < nnodes; i++) {
						set_reset_send_all(i, plus_minus);
					}
				}
				else {
					nodeid_2 = lookup(operand_2);
					set_reset_receive_all(nodeid_2, plus_minus);
				}
			}

			else if(strcmp(operand_2, "*") == 0) {
				nodeid_1 = lookup(operand_1);
				set_reset_send_all(nodeid_1, plus_minus);
			}

			else {
				nodeid_1 = lookup(operand_1);
				nodeid_2 = lookup(operand_2);
				srbit(nodes[nodeid_1].nmcast, nodeid_2, plus_minus);
				if(symmetric > 0) {
					srbit(nodes[nodeid_2].nmcast, nodeid_1, plus_minus);
				}
			}
			scanf("%[^\n]", update_string);
			getchar();
		}

		/* Analyze the results */

		for (int nindex=0; nindex<nnodes; nindex++) {
			sptr = &nodes[nindex];
			printf("Node %2d ", nindex);
			msg="Can send & receive";
			if (sptr->nsend == 0) {
				msg = "Can only receive";
				if (sptr->nrecv == 0) {
					msg = "Completely isolated";
				}
			} else if (sptr->nrecv == 0) {
				msg = "Can only send";
			}
			printf(" %-19s",msg);
			printf(" (original name %s)\n", sptr->nname);
			printf("         Multicast address: ");
			for (i=0;i<6;i++) {
				printf(" ");
				for (j=7; j>=0; j--) {
					printf("%d",(sptr->nmcast[i]>>j)&0x01);
				}
			}
			printf("\n");
		}

		/* Output a topology database */

		outfile = malloc(strlen(argv[1])+3);
		strcpy(outfile, argv[1]);
		strcat(outfile, ".1");
		if ( (fout = fopen(outfile, "w") ) == NULL) {
			fprintf(stderr,"error - cannot open output file %s\n", outfile);
		}

		/* Write the multicast address for each node */

		for (nindex=0; nindex<nnodes; nindex++) {
			fwrite(nodes[nindex].nmcast, 1, 6, fout);
		}

		/* Write the sentinel value */

		sentinel[0] = sentinel[1] = sentinel[2] = sentinel[3] =
			sentinel[4] = sentinel[5] = 0x00;
		fwrite(sentinel, 1, 6, fout);

		/* Write the node names as a 1-byte length field followed by	*/
		/*	a set of 9null-terminated) characters that form the	*/
		/*	name of the node.  The length includes the null byte.	*/

		for (nindex=0; nindex<nnodes; nindex++) {
			nlen = (strlen(nodes[nindex].nname) + 1) & 0xff;
			fwrite(&nlen, 1, 1, fout);
			fwrite(nodes[nindex].nname, 1, nlen, fout);
		}

		fclose(fout);
	}
	exit(0);
}
