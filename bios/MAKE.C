#include	<stdio.h>
#include	<fcntl.h>
#include	<ctype.h>

#define	DEFAULT	 "makefile"
#define	EXECFILE "execmake.sub"
#define	INMAX	130
#define	TRUE	1
#define	FALSE	0

struct	idate {			/* internal representation   */
	int	Days;		/* same size as a 'long'     */	
	char	Hours;
	char	Minutes;
};

struct	howrec	{
	char	*howcom;
	struct	howrec	*nexthow;
};

struct	deprec	{
	char	*name;
	struct	defnrec	*def;
	struct	deprec	*nextdep;
};

struct	defnrec	{
	char	*name;
	int	uptodate;
	unsigned long	modified;
	struct	deprec	*dependson;
	struct	howrec	*howto;
	struct	defnrec	*nextdefn;
};

struct	dorec	{
	char	*name;
	struct	dorec	*nextdo;
};

struct	defnrec	*defnlist;
struct	dorec	*dolist;

int	execute;
int	madesomething;
int	knowhow;
int	listinp;

FILE	*mfp;

main( argc, argv )
int	argc;
char	*argv[];
{
	long	make();
	char	*makename;
Š	init( argc, argv );

	/* create submit file if needed */
	unlink( EXECFILE );
	if( execute ) {
		if( (mfp = fopen( EXECFILE, "w")) == 0 ) {
			fprintf( stderr, "can't create execute file.\n" );
			exit( 1 );
		}
	}
	/* if no name was given use the first definition */
	if( dolist == NULL ) {
		dolist = (struct dorec *) malloc( sizeof( struct dorec ) );
		dolist->name = defnlist->name;
		dolist->nextdo = NULL;
	}

	/* remember what we are making */
	makename = dolist->name;

	/* now fall down the dolist and do them all */
	while( dolist != NULL ) {
		madesomething = FALSE;
		make( dolist->name );	/* ignore return value */
		if( !madesomething ) {
			if( knowhow )
				fprintf( stderr, "'%s' is up to date\n", dolist->name );
			else
				fprintf( stderr, "don't know how to make '%s'\n", dolist->name );
		}
		dolist = dolist->nextdo;
	}

	/* see if we made anything */
	if( execute && madesomething ) {
		fprintf( mfp, "erase %s\n", EXECFILE );
		fclose( mfp );
		sprintf( 0x0080, "SUBMIT %s\r", EXECFILE );
		bdos( 47, 0 );
	}
	else
		unlink( EXECFILE );
}

init( argc, argv )
int	argc;
char	*argv[];
{
	int	i, usedefault;

	dolist = NULL;
	defnlist = NULL;
	
	usedefault = execute = TRUE;
	listinp = FALSE;

	for( i = 1; i < argc; ++i ) {
		if( argv[i][0] == '-' ) {
			/* option */
			switch( toupper( argv[i][1] ) ) {
			case 'F':
				if( ++i < argc ) {
					if( readmakefile( argv[i] ) == FALSE )
						exit( 1 );
					usedefault = FALSE;
				}
				else
					usage();
				break;

			case 'N':
				execute = FALSE;	break;

			case 'L':
				listinp = TRUE;		break;

			default:
				usage();		break;
			}
		}
		else
			/* add this file to do list */
			add_to( argv[i] );
	}
	if( usedefault )
		if( readmakefile( DEFAULT ) == FALSE )
			exit( 1 );
}

usage()			/* explai how to use program */
{
	fprintf( stderr, "usage: make [-n] [-l] [-f makefile] [filename ...]\n" );
	exit( 1 );
}

long	make(s)		/* returns the modified date/time */
char	*s;
{
	struct	defnrec	*defnp;
	struct	deprec	*depp;
	struct	howrec	*howp;
	unsigned long	latest, FileTime(), max(), CurrTime();

	/* look for the definition */
	defnp = defnlist;
	while( defnp != NULL ) {
		if( strcmp( defnp->name, s ) == 0 )
			break;
		defnp = defnp->nextdefn;
	}

	if( defnp == NULL ) {
		/* don't know how to make it */
		knowhow = FALSE;
		latest = FileTime( s );
		if( latest == 0 ) {
			/* doesn't exist but don't know how to make it */
			fprintf( stderr, "can't make '%s'\n", s );
			exit( 1 );
		}
		else
			return( latest );Š	}

	if( defnp->uptodate )
		return( defnp->modified );

	/* make sure everything it depends on is up to date */
	latest = 0;
	depp = defnp->dependson;
	while( depp != NULL ) {
		latest = max( make( depp->name ), latest );
		depp = depp->nextdep;
	}

	/* has dependencies therefore we know how */
	knowhow = TRUE;

	/* if necessary, execute all of the commands to make it */
	/* if ( out of date ) || ( depends on nothing )         */
	if( (latest >= defnp->modified) || (defnp->dependson == NULL) ) {
		/* make these guys */
		howp = defnp->howto;
		while( howp != NULL ) {
			printf( "%s\n", howp->howcom );
			if( execute )
				/* add line to create file to the submit file */
				fprintf( mfp, "%s\n", howp->howcom );
			howp = howp->nexthow;
		}
		/* file has now been modified */
		defnp->modified = CurrTime();
		defnp->uptodate = TRUE;
		if( defnp->howto != NULL )
			/* we had instructions */
			madesomething = TRUE;
	}

	return( defnp->modified );
}

add_to( s )		/* add filename to do list */
char	*s;
{
	struct	dorec	*ptr1, *ptr2;

	ptr1 = (struct dorec *) malloc( sizeof( struct dorec ) );

	ptr1->name = s;	/* okay since only called with an argv */
	ptr1->nextdo = NULL;Š
	/* make sure we can always match (case not important) */
	lowercase( ptr1->name );

	/* now go down the dolist */
	if( dolist == NULL )
		dolist = ptr1;
	else {
		ptr2 = dolist;
		while( ptr2->nextdo != NULL )
			ptr2 = ptr2->nextdo;
		ptr2->nextdo = ptr1;
	}
}

readmakefile( s )		/* process specified makefile */
char	*s;
{
	FILE	*fil;
	int	doneline, pos, i, j;
	char	inline[INMAX], info[INMAX];
	struct	defnrec	*defnp, *defnp2;
	struct	deprec	*depp, *depp2;
	struct	howrec	*howp, *howp2;
	char	*ListTime();
	char	ws[30];		/* string for date/time */

	/* try to open spcified file */
	if( (fil = fopen( s, "r" )) == 0 ) {
		fprintf( stderr, "couldn't open '%s'\n", s );
		return( FALSE );
	}

	while( fgets( inline, INMAX, fil ) != NULL ) {
		inline[strlen(inline)-1] = '\0';	/* strip newline */

		if( inline[0] == '\0' )
			/* ignore blank lines */
			continue;

		if( inline[0] != '\t' ) {
			/* start of new def */
			lowercase( inline );
			/* get what we're defining into info */
			if( sscanf( inline, "%s ", info ) != 1 ) {
				fprintf( stderr, "can't scan '%s'\n", inline );
				continue;
			}

			/* get a new struct */
			defnp = (struct defnrec *) malloc( sizeof( struct defnrec ) );
			/* add it to the end of defnlist */
			if( defnlist == NULL )
				defnlist = defnp;
			else {
				defnp2 = defnlist;
				while( defnp2->nextdefn != NULL )
					defnp2 = defnp2->nextdefn;
				defnp2->nextdefn = defnp;Š			}

			/* initialize it */
			defnp->name = (char *) malloc( strlen( info ) + 1 );
			strcpy( defnp->name, info );
			defnp->uptodate = FALSE;	/* actually unknown */
			defnp->modified = FileTime( defnp->name );
			if( listinp ) {
				fprintf( stderr, "\nfile = '%s', last modified %s\n",
					defnp->name, ListTime( defnp->modified, ws ) );
				fprintf( stderr, "depends on:\n" );
			}
			defnp->dependson = NULL;
			defnp->howto = NULL;
			defnp->nextdefn = NULL;

			/* now go thru all of it's dependencies */
			/* first, move past the first name */
			pos = 0;
			while( isspace( inline[pos] ) )
				++pos;
			while( !isspace( inline[pos] ) &&  inline[pos] != '\0' )
				++pos;
			/* now loop thru the names */
			doneline = FALSE;
			while( !doneline ) {
		scandep:
				while( isspace( inline[pos] ) )
					++pos;
				if( inline[pos] == '\0' ) {
					doneline = TRUE;
					continue;
				}
				for( i = 0; !isspace( inline[pos] ) && inline[pos] != '\0'; )
					info[i++] = inline[pos++];
				info[i] = '\0';
				/* see if line is continued onto next real line */
				if( strcmp( info, "\\" ) == 0 ) {
					/* read next line */
					fgets( inline, INMAX, fil );
					inline[strlen(inline)-1] = '\0';
					lowercase( inline );
					pos = 0;
					goto scandep;
				}
				/* get a new struct */
				depp = (struct deprec *) malloc( strlen( info ) + 1 );
				/* add it to the end of deplist */
				if( defnp->dependson == NULL )
					defnp->dependson = depp;
				else {
					depp2 = defnp->dependson;
					while( depp2->nextdep != NULL )
						depp2 = depp2->nextdep;
					depp2->nextdep = depp;
				}Š				depp->name = (char *) malloc( strlen( info ) + 1 );
				strcpy( depp->name, info );
				if( listinp )
					fprintf( stderr, "\t%s\n", depp->name );
				depp->nextdep = NULL;
			}
		}
		else {	/* a how to line */
			if( defnp == NULL ) {
				fprintf( stderr, "howto line w/o definition\n" );
				fprintf( stderr, "\t'%s'\n", inline );
			}
			/* now allocate space for the command/args line */
			for( pos = 0; isspace( inline[pos] ); ++pos );

			/* if there is something there, allocate mem and copy */
			if( strlen( &inline[pos] ) != 0 ) {
				/* get a new struct */
				howp = (struct howrec *) malloc( sizeof( struct howrec ) );
				/* add it to the end of howlist */
				if( defnp->howto == NULL ) {
					defnp->howto = howp;
					if( listinp )
						fprintf( stderr, "how to make:\n" );
				}
				else {
					howp2 = defnp->howto;
					while( howp2->nexthow != NULL )
						howp2 = howp2->nexthow;
					howp2->nexthow = howp;
				}
				/* copy command line */
				howp->howcom = (char *) malloc( strlen( inline ) - pos + 1 );
				for( i = 0; inline[pos] != '\0'; )
					howp->howcom[i++] = inline[pos++];
				howp->howcom[i] = '\0';
				if( listinp )
					fprintf( stderr, "\t%s\n", howp->howcom );
				howp->nexthow = NULL;
			}
		}
	}
	return( TRUE );
}

lowercase( s )
char	*s;
{
	for( ; *s != '\0'; s++ )
		*s = tolower( *s );
}

unsigned long	max( a, b )
unsigned long	a, b;
{
	return( a > b ? a : b );Š}

unsigned long	FileTime( fname )
char	*fname;
{
	unsigned long	tolong();
	char	 fcb[36];

	fcbinit( fname, fcb );
	setmem( &fcb[16], 16, NULL );
	bdos( 102, fcb );
	return( tolong( &fcb[28] ) );
}

unsigned long	CurrTime()
{
	unsigned long	tolong();
	struct	 idate	time;

	bdos( 105, &time );
	return( tolong( &time ) );
}

unsigned long	tolong( cp )
char	 cp[];
{
	unsigned long	work;
	register char	*wp;

	wp = &work;

	*wp++ = cp[3];	*wp++ = cp[2];
	*wp++ = cp[0];	*wp   = cp[1];

	return( work );
}

todate( fp, tp )
char	 fp[];
register char	*tp;
{
	*tp++ = fp[2];	*tp++ = fp[3];
	*tp++ = fp[1];	*tp   = fp[0];
}

char	*ListTime( val, dp )	/* convert long to readable date/time */
unsigned long	val;
register char	*dp;
{
register char	*sp;
static	struct	idate InternalDate;
static	char	*DayOfWeek[] = {
	"Sat ", "Sun ", "Mon ", "Tue ", "Wed ", "Thu ", "Fri "
};
static	int	MonthCount[] = {
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};
static	 int	years, months, days, ddays;

	/* move the day of week string */
	todate( &val, &InternalDate );
	for( sp = DayOfWeek[InternalDate.Days % 7]; *sp; *dp++ = *sp++ );

	/* compute the Date and move into ExternalDate */
	/* first calculate the current year */
	for( years = 78, days = InternalDate.Days;
	     days > (ddays = (years & 3) ? 365 : 366);
	     ++years, days -= ddays ) ;

	/* days now contains the remaining days in the year */
	MonthCount[1] = ddays - 337;	/* 28 or 29 for Feb */

	/* figure out which month this is */
	for( months = 0; days > MonthCount[months]; days -= MonthCount[months++] );
	++months;		/* convert to real world number */Š
	/* move the date into the ExternalDate string */
	*dp++ = (months / 10) + '0';	*dp++ = (months % 10) + '0';	*dp++ = '/';
	*dp++ = (days / 10) + '0';	*dp++ = (days % 10) + '0';	*dp++ = '/';
	*dp++ = (years / 10) + '0';	*dp++ = (years % 10) + '0';	*dp++ = ' ';

	/* move the time into the ExternalDate string */
	*dp++ = ((InternalDate.Hours >> 4) & 0x0f) + '0';
	*dp++ = (InternalDate.Hours & 0x0f) + '0';
	*dp++ = ':';
	*dp++ = ((InternalDate.Minutes >> 4) & 0x0f) + '0';
	*dp++ = (InternalDate.Minutes & 0x0f) + '0';
	*dp   = NULL;

	return( dp - 18 );
}
