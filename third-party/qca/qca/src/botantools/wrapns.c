/*
 Copyright (C) 2006 Justin Karneges <justin@affinix.com>

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_file(const char *fname)
{
	FILE *f;
	char *buf;
	int size;

	f = fopen(fname, "r");
	if(!f)
		return 0;

	fseek(f, 0l, SEEK_END);
	size = ftell(f);
	rewind(f);
	buf = malloc(size + 1);
	if(!buf)
	{
		fclose(f);
		return 0;
	}

	fread(buf, size, 1, f);
	buf[size] = 0;
	fclose(f);

	return buf;
}

static void write_file(const char *fname, const char *buf)
{
	FILE *f;

	f = fopen(fname, "w");
	fwrite(buf, strlen(buf), 1, f);
	fclose(f);
}

static char *insert_string(char *buf, char *str, int at)
{
	int bsize, slen;
	bsize = strlen(buf) + 1;
	slen = strlen(str);

	buf = realloc(buf, bsize + slen);
	if(!buf)
		return 0;
	memmove(buf + at + slen, buf + at, bsize - at);
	memcpy(buf + at, str, slen);
	return buf;
}

static int is_include(const char *buf)
{
	char *p, *sub;
	int len;

	if(buf[0] != '#')
		return 0;

	p = strchr(buf, '\n');
	if(!p)
		return 0;

	// take the substring
	++buf;
	len = p - buf;
	sub = malloc(len + 1);
	memcpy(sub, buf, len);
	sub[len] = 0;

	if(strstr(sub, "include"))
	{
		free(sub);
		return 1;
	}
	free(sub);
	return 0;
}

static const char *find_include(const char *buf)
{
	const char *p = buf;

	if(p[0] == '#')
	{
		if(is_include(p))
			return p;
	}

	while(1)
	{
		p = strstr(p, "\n#");
		if(!p)
			break;
		++p;
		if(is_include(p))
			return p;
	}

	return 0;
}

static const char *find_std(const char *buf)
{
	const char *p;
	p = strstr(buf, "namespace std");
	return p;
}

static const char *skip_to_next_curly(const char *buf)
{
	int n;
	for(n = 0; buf[n]; ++n)
	{
		if(buf[n] == '{' || buf[n] == '}')
			return (buf + n);
	}
	return 0;
}

static const char *skip_over_curlies(const char *buf)
{
	const char *p;
	int opened;

	p = strchr(buf, '{');
	if(!p)
		return buf;

	++p;
	opened = 1;
	while(opened)
	{
		p = skip_to_next_curly(p);
		if(!p)
			return 0;
		if(*p == '{')
			++opened;
		else if(*p == '}')
			--opened;
		++p;
	}
	return p;
}

void do_it(char *buf, char *ns)
{
	char str[256], str_end[256];
	const char *p, *p2;
	int slen, slen_end;
	int at;

	sprintf(str, "namespace %s { // WRAPNS_LINE\n", ns);
	slen = strlen(str);

	sprintf(str_end, "} // WRAPNS_LINE\n", ns);
	slen_end = strlen(str_end);

	at = 0;
	while(1)
	{
		// make sure there is a line left
		p = strchr(buf + at, '\n');
		if(!p)
			break;

		// open the namespace
		buf = insert_string(buf, str, at);
		at += slen;

		// find an #include, "namespace std", or the end
		int f = 0;
		p = find_include(buf + at);
		p2 = find_std(buf + at);
		if(p && (!p2 || p < p2))
		{
			f = 1;
		}
		else if(p2)
		{
			f = 2;
			p = p2;
		}

		if(f == 0)
		{
			// point to the end
			at = strlen(buf);
		}
		else if(f == 1)
		{
			printf("found include\n");
			at = p - buf;
		}
		else if(f == 2)
		{
			printf("found std\n");
			at = p - buf;
		}

		// close it
		buf = insert_string(buf, str_end, at);
		at += slen_end;

		if(f == 1)
		{
			// go to next line
			p = strchr(buf + at, '\n');
			if(!p)
				break;
			at = p - buf + 1;
		}
		else if(f == 2)
		{
			p = skip_over_curlies(buf + at);
			if(!p)
				break;
			at = p - buf;

			// go to next line
			p = strchr(buf + at, '\n');
			if(!p)
				break;
			at = p - buf + 1;
		}
	}
}

int main(int argc, char **argv)
{
	char *buf;

	if(argc < 3)
	{
		printf("usage: wrapns [file] [namespace]\n\n");
		return 1;
	}

	buf = read_file(argv[1]);
	do_it(buf, argv[2]);
	write_file(argv[1], buf);
	free(buf);

	return 0;
}

