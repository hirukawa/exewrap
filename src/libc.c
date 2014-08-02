#include <Windows.h>

/*
size_t strlen(const char* s)
{
	return lstrlenA(s);
}

char* strcpy(char* dest, const char* src)
{
	return lstrcpyA(dest, src);
}

char* strcat(char* dest, const char* src)
{
	return lstrcatA(dest, src);
}

int strcmp(const char* s1, const char* s2)
{
	return lstrcmpA(s1, s2);
}

int stricmp(const char* s1, const char* s2)
{
	return lstrcmpiA(s1, s2);
}
*/

void* memcpy(void* dst, const void* src, size_t size)
{
	char* _dst = (char*)dst;
	const char* _src = (char*)src;
	size_t i;

	for(i = 0; i < size; i++)
	{
		_dst[i] = _src[i];
	}
	return dst;
}

void* memset(void* dst, int val, size_t size)
{
	char* realdst = (char*)dst;
	size_t i;

	for(i = 0; i < size; i++)
	{
		realdst[i] = (char)val;
	}
	return dst;
}


char* lstrchr(const char* s, int c)
{
	while(*s)
	{
		if (*s == c)
		{
			return (char*)s;
		}
		s++;
	}

	return NULL;
}

char* lstrrchr(const char* s, int c)
{
	const char* tmp = s + lstrlenA(s) - 1;

	while(tmp >= s)
	{
		if(*tmp == c)
		{
			return (char*)tmp;
		}
		else
		{
			tmp--;
		}
	}

	return NULL;
}

char* lstrstr(const char* haystack, const char* needle)
{
	int haystack_len = lstrlenA(haystack);
	int needle_len = lstrlenA(needle);
	int i;

	if(needle_len == 0)
	{
		return (char*)haystack;
	}
	if(haystack_len < needle_len)
	{
		return 0;
	}
	for(i = 0; i < haystack_len - needle_len + 1; i++)
	{
		if (!lstrcmpA(&haystack[i], needle))
		{
			return (char*)(&haystack[i]);
		}
	}

	return NULL;
}

char* lstrtok(char* str, const char* delim)
{
	char* spanp;
	int c;
	int sc;
	char* tok;
	static char* last;

	if(str == NULL && (str = last) == NULL)
	{
		return NULL;
	}

cont:
	c = *str++;
	for(spanp = (char*)delim; (sc = *spanp++) != 0;)
	{
		if(c == sc)
		{
			goto cont;
		}
	}

	if(c == 0)
	{
		last = NULL;
		return NULL;
	}
	tok = str - 1;

	for(;;)
	{
		c = *str++;
		spanp = (char*)delim;
		do {
			if((sc = *spanp++) == c)
			{
				if(c == 0)
				{
					str = NULL;
				}
				else
				{
					str[-1] = 0;
				}
				last = str;
				return tok;
			}
		} while(sc != 0);
	}
}

char* lstrupr(char* s)
{
	char* r = s;

	while(*s != '\0')
	{
		if('a' <= *s && *s <= 'z')
		{
			*s -= ('a' - 'A');
		}
		s++;
	}

	return r;
}

int printf(const char* format, ...)
{
    va_list args;
	char*   buf;
	int     size;
	DWORD   written;

    va_start(args, format);
	buf = (char*)HeapAlloc(GetProcessHeap(), 0, 8192);
	size = wvsprintfA(buf, format, args);
	WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), buf, size, &written, NULL); 
    va_end(args);
	HeapFree(GetProcessHeap(), 0, buf);

	return size;
}

int sprintf(char* str, const char* format, ...)
{
    va_list args;
    int ret;
          
    va_start(args, format);
    ret = wvsprintfA(str, format, args);
    va_end(args);

    return ret;
}

int atoi(const char* nptr)
{
    return (int)atol(nptr);
}

long atol(const char* nptr)
{
    int cur;
    int neg;
	long total = 0;

	while((*nptr >= 0x09 && *nptr <= 0x0D) || (*nptr == 0x20))
	{
        nptr++;
	}

    neg = cur = *nptr++;

    if(cur == '-' || cur == '+')
	{
        cur = *nptr++;
	}

    while(('0' <= cur) && (cur <= '9'))
    {
        total = 10 * total + (cur - '0');
        cur = *nptr++;
    }

    if(neg == '-')
	{
        return -total;
	}
    else
	{
        return total;
	}
}

