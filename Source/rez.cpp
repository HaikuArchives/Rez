/*	$Id: rez.cpp,v 1.1.1.1 2000/03/05 06:22:46 tpv Exp $
	
	Copyright 1996, 1997, 1998
	        Hekkelman Programmatuur B.V.  All rights reserved.
	
	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:
	1. Redistributions of source code must retain the above copyright notice,
	   this list of conditions and the following disclaimer.
	2. Redistributions in binary form must reproduce the above copyright notice,
	   this list of conditions and the following disclaimer in the documentation
	   and/or other materials provided with the distribution.
	3. All advertising materials mentioning features or use of this software
	   must display the following acknowledgement:
	   
	    This product includes software developed by Hekkelman Programmatuur B.V.
	
	4. The name of Hekkelman Programmatuur B.V. may not be used to endorse or
	   promote products derived from this software without specific prior
	   written permission.
	
	THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
	INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
	FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
	AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
	EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
	PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
	OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
	WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
	OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
	ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 	

	Created: 12/02/98 15:37:15
*/

/*************************************************************************
 * Modifications to the standard REZ distribution, by Tim Vernum
 * Copyright (c) 2000, Tim Vernum
 * This code may be freely used for any purpose
 *************************************************************************
 * Added "parse-only" option
 * Added "generate-dependencies" option
 *************************************************************************/

#include "rez.h"

#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <cstdio>
#include <StorageKit.h>
#include <cstring>
#include <cstdlib>

const int kMaxIncludePaths = 20;

extern int yyparse();
extern FILE *yyin;
extern int yylineno;
extern int yydebug;


int verbose = 0;

int gResID, gResType;		// globals that describe the current resource
char *gResName;
void *gResData;
int gResSize;
char *gIncludePaths[kMaxIncludePaths];

char out[NAME_MAX] = "rez.out";
char *in = NULL;
static FILE *gHeader;

static bool gSaveAsHeader = false;
static bool gTruncate = false;
static bool gDump = false;
static BResources *resFile;

void Usage();
int getoptions(int argc, char *argv[]);
void Work(const char *file);

void Usage()
{
	puts("Usage: rez [-vhtdmp] [-I path] [-o outfile] file\n"
		"    -v         increment verbose level\n"
		"    -o file    specify output filename, default is rez.out\n"
		"    -h         write resource as C struct in a header file\n"
		"    -t         truncate resource file\n"
		"    -d         Dump resource to stdout\n"
		"    -I path    Include paths\n"
		"    -m         List dependencies for Makefile (extension)\n"
		"    -p         Parse only (extension)\n"
		"    file(s)    resource definition file(s)");
	exit(1);
} /* Usage */

int getoptions(int argc, char *argv[])
{
	int i = 1, icnt = 0;
	char **incl = gIncludePaths;
	
	*incl++ = "";
	*incl++ = ".";

	if (argc == 1)
		Usage();
	
	while (i < argc)
	{
		if (argv[i][0] == '-')
		{
//			if (strlen(argv[i]) != 2)
//				Usage();

			switch (argv[i][1])
			{
				case 'v':	verbose++; if (verbose > 2) yydebug = true; break;
				case 't':	gTruncate = true; break;
				case 'y':	yydebug = 1; break;
				case 'h':	gSaveAsHeader = true; break;
				case 'o':	i++; strcpy(out, argv[i]); break;
				case 'd':	gDump = true; break;
				case 'I':
					if (strlen(argv[i]) == 2)
					{
						i++;
						*incl++ = argv[i];
					}
					else
						*incl ++ = argv[i] + 2;
						
					if (++icnt >= 19)
						rez_error("too many include paths");
					break;
				case 'm':
					gListDepend = true ;
					break ;
				case 'p':
					gParseOnly = true ;
					break ;
				default:	Usage();
			}
		}
		else
			break;

		i++;
	}
	
	return i;
} /* getopt */

void rez_error(const char *e, ...)
{
	char msg[1024] = "### Rez Error\n# ";
	
	va_list vl;
	va_start(vl, e);
	vsprintf(msg + strlen(msg), e, vl);
	va_end(vl);

	fputs(msg, stderr);
	if (in) fprintf(stderr, "\n#------------\nFile \"%s\"; Line %d\n#-------------\n", in, yylineno);
	fflush(stderr);
	exit(1);
} /* error */

void rez_warn(const char *e, ...)
{
	char msg[1024] = "### Rez Warning\n# ";
	
	va_list vl;
	va_start(vl, e);
	vsprintf(msg + strlen(msg), e, vl);
	va_end(vl);

	fputs(msg, stderr);
	if (in) fprintf(stderr, "\n#------------\nFile \"%s\"; Line %d\n#-------------\n", in, yylineno);
} /* warn */

void Work(const char *file)
{
	StartDepend( out ) ;
	DependFile( file ) ;
	
	if (file)
		yyin = fopen(file, "r");
	
	if (yyin == NULL)
		rez_error("Error opening file %s", file ? file : "stdin");
	
	while (yyparse() == 0)
		;
	
	fclose(yyin);

	EndDepend( ) ;
} /* Work */

main(int argc, char *argv[])
{
	yyin = stdin;

	int i = getoptions(argc, argv);
		
	BFile f;
	BResources res;
	if( !gParseOnly )
	{
		char buf[PATH_MAX];
		getcwd(buf, PATH_MAX);
		strcat(buf, "/");
		strcat(buf, out);
		
		BEntry e;
		if (e.SetTo(out)) rez_error("entry set to %s", out);
		
		BDirectory d;
		if (e.GetParent(&d)) rez_error("get parent of %s", out);
		if ((gTruncate || gSaveAsHeader) && e.Exists() && e.Remove())
			rez_error("removing %s", out);
	
		
		if (!gDump)
		{
			if (gTruncate || !e.Exists())
			{
				if (d.CreateFile(buf, &f)) rez_error("creating %s", buf);
				gTruncate = true;
			}
			else	
				if (f.SetTo(buf, B_READ_WRITE)) rez_error("opening %s", buf);
			
			if (gSaveAsHeader)
			{
				gHeader = fopen(buf, "w");
				if (!gHeader) rez_error("Error creating %s", buf);
			}
			else if ( res.SetTo(&f, gTruncate) != B_NO_ERROR)
				rez_error("opening resource file %s", buf);
		}
		
		resFile = &res;
	}
	if (i == argc)
		Work(NULL);
	else
	{
		while (i < argc)
			Work(in = argv[i++]);
	}

	if (verbose)
		puts("done");

	if (gHeader)
		fclose(gHeader);
	else
		f.Sync();

	return 0;
} /* main */

#pragma mark -

ResHeader::ResHeader(int t, int i, int n)
{
	type = t;
	id = i;
	if (n)
		name = strdup((char *)n);
	else
		name = NULL;
	
	gResID = id;
	gResType = type;
	gResName = name;
} /* ResHeader::ResHeader */

void WriteResource(int x)
{
	ResHeader *rh = (ResHeader *)x;

	long sType = htonl(rh->type);
	if (verbose) printf("Writing Resource. Type: %4.4s, id: %d, name: %s\n",
		&sType, rh->id, rh->name);

	if( !gParseOnly )
	{
		if (gDump)
			fwrite(gResData, gResSize, 1, stdout);
		else if (gSaveAsHeader)
			WriteHeader(rh->type, rh->id, (unsigned char *)gResData, gResSize, rh->name);
		else
		{
			if (resFile->HasResource(rh->type, rh->id))
				resFile->RemoveResource(rh->type, rh->id);
	
			if (resFile->AddResource(rh->type, rh->id, gResData, gResSize, rh->name))
				rez_error("writing resource");
		}
	}
		
	gResSize = 0;
	free(gResData);
	gResData = NULL;
} /* WriteResource */

void WriteResource(const char *file, int type, int id, const char *name)
{
	void *p;
	size_t s;
	FILE *f;
	char **i = gIncludePaths, path[PATH_MAX];
	
	do
	{
		strcpy(path, *i++);
		strcat(path, "/");
		strcat(path, file);
	
		f = fopen(path, "rb");
	}
	while (f == NULL && *i);
	
	if (!f) rez_error("Error opening file %s: %s", file, strerror(errno));
	long sType = htonl(type);
	if (verbose) printf("Writing Resource. Type: %4.4s, id: %d, name: %s\n", &sType, id, name);

	fseek(f, 0, SEEK_END);
	s = ftell(f);
	fseek(f, 0, SEEK_SET);
	p = malloc(s);
	if (!p) rez_error("Insufficient memory");
	
	fread(p, s, 1, f);
	fclose(f);
	
	if( !gParseOnly )
	{
		if (gDump)
			fwrite(p, s, 1, stdout);
		else if (gSaveAsHeader)
			WriteHeader(type, id, (unsigned char *)p, s, name);
		else
		{
			if (resFile->HasResource(type, id))
				resFile->RemoveResource(type, id);
	
			if (resFile->AddResource(type, id, p, s, name))
				rez_error("writing resource");
		}
	}	
	DependFile( file ) ;
	free(p);
} /* WriteResource */

const char hexstring[] = "0123456789abcdef";

void WriteHeader(unsigned long type, int id, const unsigned char *buf,
	int bufSize, const char *name)
{
	const unsigned char *p = buf;
	int b = bufSize;
	long aType = htonl(type);

	fprintf(gHeader, "const char\n\tk%4.4s%d[%d] = {", &aType, id, bufSize);
	
	while (bufSize > 0)
	{
		int m = min_c(bufSize, 16);

		fprintf(gHeader, "\n\t\t");

		for (int j = 0; j < m; j++, bufSize--)
			fprintf(gHeader, "0x%02x, ", *p++);
	}
	
	fprintf(gHeader, "\n\t};\nconst int k%4.4s%dSize = %d;\n\n", &aType, id, b);
} /* WriteHeader */

void Include(const char *file)
{
	if( !gParseOnly )
	{
		BFile f;
		char **i = gIncludePaths, path[PATH_MAX];
		
		do
		{
			strcpy(path, *i++);
			strcat(path, "/");
			strcat(path, file);
		}
		while (f.SetTo(path, B_READ_ONLY) != B_OK && *i);
		
		if (f.InitCheck() != B_OK) rez_error("Error opening file %s: %s", file, strerror(errno));
		
		if (resFile)
			resFile->MergeFrom(&f);
		else
			rez_error("Should write to a resource file for Include to work!");
	}
		
	DependFile( file ) ;
} // Include
