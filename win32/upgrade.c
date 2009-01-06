//#include <windows.h>
//#include <direct.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
//#include <sys/types.h>
//#include <sys/stat.h>
//#include <errno.h>

/*static void generic_error(int codenum)
{
	char str[128];
	sprintf(str, "There was a problem during the Barracuda IM client "
		"upgrade.  Code: %02d", codenum);
	MessageBox(NULL, str, "Error", MB_OK);
}

// sfxcmd example: C:\foo\bar\selfextractor.exe
// this function chops off the last backslash and everything after it,
//   returning the result as a newly allocated string.
// example result: C:\foo\bar
static char *parse_sfxcmd_path(const char *str)
{
	int at;
	for(at = strlen(str) - 1; at >= 0; --at)
	{
		if(str[at] == '\\')
		{
			char *out = malloc(at + 1);
			if(!out)
				return NULL;
			memcpy(out, str, at);
			out[at] = 0;
			return out;
		}
	}
	return NULL;
}

static char *dequote(const char *str)
{
	char *out;
	int slen;
	int start;
	int end;
	int len;

	slen = strlen(str);

	start = 0;
	end = slen - 1;
	if(slen > 0 && str[0] == '\"')
		++start;

	if(end > start && str[end] == '\"')
		--end;

	// this works even if end is -1
	len = end - start + 1;

	out = malloc(len + 1);
	if(!out)
		return NULL;

	memcpy(out, str + start, len);
	out[len] = 0;
	return out;
}

static char *append_str(const char *str, const char *add)
{
	char *out;
	int slen, alen;

	slen = strlen(str);
	alen = strlen(add);
	out = malloc(slen + alen + 1);
	if(!out)
		return NULL;

	memcpy(out, str, slen);
	memcpy(out + slen, add, alen);
	out[slen + alen] = 0;
	return out;
}

// copy the string, with appended backslash if necessary
static char *ensure_backslash(const char *str)
{
	char *out;
	int slen;

	slen = strlen(str);
	if(slen > 0 && str[slen - 1] == '\\')
		return strdup(str);

	out = malloc(slen + 2); // backslash + null terminator
	if(!out)
		return NULL;

	memcpy(out, str, slen);
	out[slen] = '\\';
	out[slen + 1] = 0;
	return out;
}

static int cudapath_isvalid(const char *str)
{
	char *fname = "Barracuda.exe";
	int plen = strlen(str);
	int flen = strlen(fname);
	char *tmp;
	struct _stat buf;
	int ret;

	tmp = malloc(plen + flen + 1);
	if(!tmp)
		return 0;

	memcpy(tmp, str, plen);
	memcpy(tmp + plen, fname, flen);
	tmp[plen + flen] = 0;

	ret = _stat(tmp, &buf);
	free(tmp);
	if(ret != 0)
		return 0;

	return 1;
}
*/

// a heap-based string object that be either 8-bit or 16-bit
//
// the basic idea is that this is kind of like windows TCHAR, except
//   that the type of string is determined at runtime
typedef struct up_string
{
	// -1 if uninitialized, 1 if unicode, 0 if local 8 bit
	int format;

	// number of characters in the string (not counting null terminator)
	int len;

	// u16 holds 16-bit unicode characters, u8 holds 8-bit locally
	//   encoded characters.  in either mode, the string is
	//   zero-terminated.
	union
	{
		unsigned short *u16;
		unsigned char *u8;
	} data;
} up_string_t;

static up_string_t *up_string_new();
static up_string_t *up_string_copy(const up_string_t *other);
static void up_string_delete(up_string_t *s);
static int up_string_set_strz16(up_string_t *s, const unsigned short *u16);
static int up_string_set_strz8(up_string_t *s, const char *u8);
static unsigned short up_string_at16(const up_string_t *s, int pos);
static unsigned char up_string_at8(const up_string_t *s, int pos);
static int up_string_append(up_string_t *s, const up_string_t *other);
static up_string_t *up_string_mid(const up_string_t *s, int start, int len);

up_string_t *up_string_new()
{
	up_string_t *s;
	s = (up_string_t *)malloc(sizeof(up_string_t));
	if(!s)
		return NULL;
	s->format = -1;
	s->len = 0;
	return s;
}

// note: this function works even for uninitialized strings
up_string_t *up_string_copy(const up_string_t *other)
{
	up_string_t *s;
	s = up_string_new();
	if(!s)
		return NULL;
	s->format = other->format;
	s->len = other->len;
	if(other->format == 1)
	{
		s->data.u16 = (unsigned short *)malloc((s->len + 1) * sizeof(unsigned short));
		if(!s->data.u16)
		{
			up_string_delete(s);
			return NULL;
		}
		memcpy(s->data.u16, other->data.u16, (s->len + 1) * sizeof(unsigned short));
	}
	else if(other->format == 0)
	{
		s->data.u8 = (unsigned char *)malloc(s->len + 1);
		if(!s->data.u8)
		{
			up_string_delete(s);
			return NULL;
		}
		memcpy(s->data.u8, other->data.u8, s->len + 1);
	}
	return s;
}

// note: this function works even for uninitialized strings
void up_string_delete(up_string_t *s)
{
	if(s->format == 1)
		free(s->data.u16);
	else if(s->format == 0)
		free(s->data.u8);
	free(s);
}

int up_string_set_strz16(up_string_t *s, const unsigned short *u16)
{
	int len;
	unsigned short *data;

	for(len = 0;; ++len)
	{
		if(u16[len] == 0)
			break;
	}

	data = (unsigned short *)malloc((len + 1) * sizeof(unsigned short));
	if(!data)
		return 0;
	memcpy(data, u16, (len + 1) * sizeof(unsigned short));

	if(s->format == 1)
		free(s->data.u16);
	else if(s->format == 0)
		free(s->data.u8);
	s->format = 1;
	s->len = len;
	s->data.u16 = data;
	return 1;
}

int up_string_set_strz8(up_string_t *s, const char *u8)
{
	int len;
	unsigned char *data;

	for(len = 0;; ++len)
	{
		if(u8[len] == 0)
			break;
	}

	data = (unsigned char *)malloc(len + 1);
	if(!data)
		return 0;
	memcpy(data, u8, len + 1);

	if(s->format == 1)
		free(s->data.u16);
	else if(s->format == 0)
		free(s->data.u8);
	s->format = 0;
	s->len = len;
	s->data.u8 = data;
	return 1;
}

unsigned short up_string_at16(const up_string_t *s, int pos)
{
	return s->data.u16[pos];
}

unsigned char up_string_at8(const up_string_t *s, int pos)
{
	return s->data.u8[pos];
}

int up_string_append(up_string_t *s, const up_string_t *other)
{
	void *data;
	int newlen;

	// if other string is uninitialized, then this is a no-op
	if(other->format == -1)
		return 1;

	// otherwise, this string must be the same format, or uninitialized
	if(s->format != -1 && s->format != other->format)
		return 0;

	// uninitialized
	if(s->format == -1)
	{
		if(other->format == 1)
		{
			data = (unsigned short *)malloc((other->len + 1) * sizeof(unsigned short));
			if(!data)
				return 0;
			memcpy(data, other->data.u16, (other->len + 1) * sizeof(unsigned short));
			s->data.u16 = (unsigned short *)data;
		}
		else
		{
			data = (unsigned char *)malloc(other->len + 1);
			if(!data)
				return 0;
			memcpy(data, other->data.u8, other->len + 1);
			s->data.u8 = (unsigned char *)data;
		}

		s->format = other->format;
		s->len = other->len;
		return 1;
	}
	else
	{
		newlen = s->len + other->len;

		if(other->format == 1)
		{
			data = (unsigned short *)realloc(s->data.u16, (newlen + 1) * sizeof(unsigned short));
			if(!data)
				return 0;
			s->data.u16 = (unsigned short *)data;
			memcpy(s->data.u16 + s->len, other->data.u16, (other->len + 1) * sizeof(unsigned short));
		}
		else
		{
			data = (unsigned char *)realloc(s->data.u8, newlen + 1);
			if(!data)
				return 0;
			s->data.u8 = (unsigned char *)data;
			memcpy(s->data.u8 + s->len, other->data.u8, other->len + 1);
		}

		s->len = newlen;
		return 1;
	}
}

up_string_t *up_string_mid(const up_string_t *s, int start, int len)
{
	up_string_t *out;
	void *data;

	// integrity check
	if(s->format == -1 || start < 0)
		return NULL;

	// keep start in range
	if(start > s->len)
		start = s->len;

	// if len is -1 or too large, just go until end of string
	if(len == -1 || start + len > s->len)
		len = s->len - start;

	out = up_string_new();
	if(!out)
		return NULL;

	out->format = s->format;
	out->len = len;
	if(s->format == 1)
	{
		data = (void *)malloc((len + 1) * sizeof(unsigned short));
		if(!data)
		{
			up_string_delete(out);
			return NULL;
		}
		memcpy(data, s->data.u16 + start, len * sizeof(unsigned short));
		((unsigned short *)data)[len] = 0;
		out->data.u16 = (unsigned short *)data;
	}
	else // 0
	{
		data = (void *)malloc(len + 1);
		if(!data)
		{
			up_string_delete(out);
			return NULL;
		}
		memcpy(data, s->data.u8 + start, len);
		((unsigned char *)data)[len] = 0;
		out->data.u8 = (unsigned char *)data;
	}

	return out;
}

int main()
{
	up_string_t *s1, *s2, *s3;

	s1 = up_string_new();
	if(!s1)
	{
		printf("Error 1\n");
	}

	if(!up_string_set_strz8(s1, "Hello World"))
	{
		printf("Error 2\n");
	}

	if(strcmp((const char *)s1->data.u8, "Hello World") != 0)
	{
		printf("Error 3\n");
		return 1;
	}

	s2 = up_string_new();
	if(!s2)
	{
		printf("Error 4\n");
		return 1;
	}

	if(!up_string_set_strz8(s2, ", What's New?"))
	{
		printf("Error 5\n");
		return 1;
	}

	if(!up_string_append(s1, s2))
	{
		printf("Error 6\n");
		return 1;
	}

	if(strcmp((const char *)s1->data.u8, "Hello World, What's New?") != 0)
	{
		printf("Error 7\n");
		return 1;
	}

	s3 = up_string_mid(s1, 0, 5);
	if(!s3)
	{
		printf("Error 8\n");
		return 1;
	}

	if(strcmp((const char *)s3->data.u8, "Hello") != 0)
	{
		printf("Error 9\n");
		return 1;
	}

	up_string_delete(s3);
	s3 = up_string_mid(s1, 6, -1);
	if(!s3)
	{
		printf("Error 10\n");
		return 1;
	}

	if(strcmp((const char *)s3->data.u8, "World, What's New?") != 0)
	{
		printf("Error 11\n");
		return 1;
	}

	up_string_delete(s1);
	s1 = up_string_copy(s3);
	if(!s1)
	{
		printf("Error 12\n");
		return 1;
	}

	up_string_delete(s3);
	up_string_delete(s2);
	s2 = up_string_mid(s1, 14, 10);
	if(!s2)
	{
		printf("Error 13\n");
		return 1;
	}

	if(strcmp((const char *)s2->data.u8, "New?") != 0)
	{
		printf("Error 14\n");
		return 1;
	}

	up_string_delete(s2);
	up_string_delete(s1);

	s1 = up_string_new();
	s2 = up_string_new();
	if(!up_string_set_strz8(s2, "FooBar"))
	{
		printf("Error 15\n");
		return 1;
	}

	if(!up_string_append(s1, s2))
	{
		printf("Error 16\n");
		return 1;
	}

	if(strcmp((const char *)s1->data.u8, "FooBar") != 0)
	{
		printf("Error 17\n");
		return 1;
	}

	up_string_delete(s2);
	up_string_delete(s1);

	printf("Success\n");

	return 0;
}

#if 0
// information about a file or directory
typedef struct up_fileinfo
{
	// name of the file or directory (including path, relative or
	//   absolute, if desired).  directories shall not have a trailing
	//   backslash.
	up_string_t *name;

	// 1 if we are representing a directory, 0 otherwise.
	int is_dir;

	// the size value here has a max of 2^31.  we cannot represent
	//   files larger than this.  our upgrader archive doesn't contain
	//   any files larger than this anyway.  if we encounter a file in
	//   the archive that appears to exceed the maximum allowed size,
	//   then the upgrade must abort.
	// if we are representing a directory, this value is 0.
	int size;
} up_fileinfo_t;

static void filename_
static file_item_t *file
// expects null-terminated list of file_item_t pointers
static void file_item_list_free(file_item_t **list)
{
	file_item_t *p;

	for(p = *list; p; ++p)
	{
		free(p->name);
		free(p);
	}
	free(list);
}

static int file_item_list_append_item(file_item_t **list, file_item_t *i)
{
	file_item_t **out;
	int n, count;

	for(count = 0; list[count]; ++count);

	out = realloc(list, sizeof(file_item_t *) * (count + 2)); // + 1 + null
	if(!out)
		return 0;

	out[count] = malloc(sizeof(file_item_t));

	// TODO: handle this error?
	//if(!out[count])

	out[count]->name = strdup(i->name);

	// TODO: handle this error?
	//if(!out[count]->name)

	out[count]->size = i->size;
	out[count]->is_dir = i->is_dir;

	out[count + 1] = NULL;

	return 1;
}

// expects 'dir' as a directory with a trailing backslash
static file_item_t **make_filelist(const char *dir)
{
	HANDLE hFind;
	WIN32_FIND_DATA findData;
	char *search;
	int error;
	file_item_t **out, **out2;
	file_item_t *i;
	int at;

	search = append_str(dir, "*");
	if(!search)
		return NULL;

	hFind = FindFirstFile(search, &findData);
	if(hFind == INVALID_HANDLE_VALUE)
		return NULL;

	out = malloc(sizeof(file_item_t *) * 2); // 1 item + null terminator
	if(!out)
		return NULL;

	// null-terminate at zero, in case of error
	out[0] = NULL;

	at = 0;
	error = 0;
	while(1)
	{
		// do something with findData.cFileName
		i = malloc(sizeof(file_item_t));
		if(!i)
		{
			error = 1;
			break;
		}

		i->name = strdup(findData.cFileName);
		if(!i->name)
		{
			free(i);
			error = 1;
			break;
		}

		// cap the file size at 31-bits
		if(findData.nFileSizeHigh == 0 && findData.nFileSizeLow <= 0x7fffffff)
			i->size = (int)findData.nFileSizeLow;
		else
			i->size = 0x7fffffff;

		if(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			i->is_dir = 1;
		else
			i->is_dir = 0;

		if(i->is_dir)
		{
			char *p = ensure_backslash(i->name);
			if(!p)
			{
				free(i);
				error = 1;
				break;
			}

			i->name = p;
		}

		out[at++] = i;
		out[at] = NULL;

		if(FindNextFile(hFind, &findData) == 0)
		{
			if(GetLastError() != ERROR_NO_MORE_FILES)
				error = 1;
			break;
		}

		out2 = realloc(out, sizeof(file_item_t *) * (at + 2)); // + 1 + null
		if(!out2)
		{
			error = 1;
			break;
		}
		out = out2;
	}

	if(error)
	{
		file_item_list_free(out);
		out = NULL;
	}

	FindClose(hFind);
	free(search);

	return out;
}

static file_item_t **make_filelist_recursive(const char *dir)
{
	file_item_t **out;
	file_item_t **cur;
	file_item_t **subdirs;
	char *subdirname;
	int n, k;

	out = malloc(sizeof(file_item_t *));
	out[0] = NULL;

	cur = make_filelist(dir);
	if(!cur)
	{
		file_item_list_free(out);
		return NULL;
	}

	for(n = 0; cur[n]; ++n)
	{
		file_item_t *i = cur[n];
		if(i->is_dir)
		{
			if(strcmp(i->name, ".\\") == 0 || strcmp(i->name, "..\\") == 0)
				continue;

			subdirname = append_str(dir, i->name);
			if(!subdirname)
			{
				file_item_list_free(cur);
				file_item_list_free(out);
				return NULL;
			}

			subdirs = make_filelist_recursive(subdirname);
			if(!subdirs)
			{
				free(subdirname);
				file_item_list_free(cur);
				file_item_list_free(out);
				return NULL;
			}

			for(k = 0; subdirs[k]; ++k)
			{
				char *tmp;
				char *p = append_str(subdirname, subdirs[k]->name);
				if(!p)
				{
					file_item_list_free(subdirs);
					free(subdirname);
					file_item_list_free(cur);
					file_item_list_free(out);
					return NULL;
				}

				// FIXME: this is pretty gross
				tmp = subdirs[k]->name;
				subdirs[k]->name = p;

				if(!file_item_list_append_item(out, subdirs[k]))
				{
					subdirs[k]->name = tmp;

					free(p);
					file_item_list_free(subdirs);
					free(subdirname);
					file_item_list_free(cur);
					file_item_list_free(out);
					return NULL;
				}

				subdirs[k]->name = tmp;
			}

			file_item_list_free(subdirs);
		}

		if(!file_item_list_append_item(out, i))
		{
			file_item_list_free(cur);
			file_item_list_free(out);
			return NULL;
		}
	}

	file_item_list_free(cur);

	return out;
}

static int copy_file(const char *fname_from, const char *fname_to)
{
	if(CopyFileA(fname_from, fname_to, FALSE) != 0)
		return 1;
	else
		return 0;
}

static int copy_files(const char *dir_from, const char *dir_to)
{
	file_item_t **list;
	char *from, *to;
	int total;
	char str[256];

	from = ensure_backslash(dir_from);
	if(!from)
		return 0;

	to = ensure_backslash(dir_to);
	if(!to)
	{
		free(from);
		return 0;
	}

	list = make_filelist_recursive(dir_from);
	if(!list)
	{
		free(to);
		free(from);
		return 0;
	}

	for(total = 0; list[total]; ++total);

	sprintf(str, "Files to copy: %d", total);
	MessageBox(NULL, str, "Copy", MB_OK);

	/*char xcopy[1024];
	sprintf(xcopy, "xcopy /E /Y \"%s\" \"%s\"", dir_from, dir_to);
	MessageBox(NULL, xcopy, "xcopy", MB_OK);
	system(xcopy);*/
	return 1;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR szCmdLine, int iCmdShow)
{
	char *p, *p2;
	char *unpackpath;
	char *sfxcmd;
	char *cudapath;
	char *newbimfpath;
	char *modpath;

	// note: for the root directory, this will end in backslash.  for any
	//   other directory, this will not end in backslash
	p = _getcwd(NULL, _MAX_PATH);
	if(!p)
	{
		generic_error(1);
		return 1;
	}

	p2 = dequote(p);
	if(!p2)
	{
		free(p);
		generic_error(2);
		return 1;
	}
	free(p);
	p = p2;

	unpackpath = ensure_backslash(p);
	if(!unpackpath)
	{
		free(p);
		generic_error(3);
		return 1;
	}
	free(p);

	sfxcmd = getenv("sfxcmd");
	if(!sfxcmd)
	{
		free(unpackpath);
		generic_error(4);
		return 1;
	}

	p = dequote(sfxcmd);
	if(!p)
	{
		free(unpackpath);
		generic_error(5);
		return 1;
	}
	sfxcmd = p;

	// note: root directory or not, this will not end in backslash
	p = parse_sfxcmd_path(sfxcmd);
	if(!p)
	{
		free(sfxcmd);
		free(unpackpath);
		generic_error(6);
		return 1;
	}
	free(sfxcmd);

	cudapath = ensure_backslash(p);
	if(!cudapath)
	{
		free(p);
		free(unpackpath);
		generic_error(7);
		return 1;
	}
	free(p);

	if(!cudapath_isvalid(cudapath))
	{
		free(cudapath);
		free(unpackpath);
		generic_error(8);
		return 1;
	}

	if(strcmp(unpackpath, cudapath) == 0)
	{
		free(cudapath);
		free(unpackpath);
		generic_error(9);
		return 1;
	}

	newbimfpath = append_str(unpackpath, "newbimf");
	if(!newbimfpath)
	{
		free(cudapath);
		free(unpackpath);
		generic_error(10);
		return 1;
	}
	free(unpackpath);

	if(!copy_files(newbimfpath, cudapath))
	{
		free(newbimfpath);
		free(cudapath);
		generic_error(11);
		return 1;
	}

	free(newbimfpath);

	/*if(_chdir(cudapath) != 0)
	{
		free(cudapath);
		generic_error(11);
		return 1;
	}*/

	MessageBox(NULL, "Your Barracuda IM client was upgraded successfully.\r\nYour client will now be restarted.", "Upgrade", MB_OK);

	modpath = append_str(cudapath, "Barracuda.exe");
	if(!modpath)
	{
		free(cudapath);
		generic_error(12);
		return 1;
	}

	if(((int)ShellExecuteA(NULL, "open", modpath, NULL, cudapath, SW_SHOWNORMAL)) <= 32)
	{
		MessageBox(NULL, "There was a problem restarting the Barracuda IM client.  Code: 13", "Error", MB_OK);
		return 1;
	}

	free(modpath);
	free(cudapath);

	return 0;
}
#endif
