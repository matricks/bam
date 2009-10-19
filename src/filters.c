#include <stdio.h>

/* global buffer for filtering */
static char buffer[1024*4];

/* just passes the data from stdin to stdout */
static int filter_pass()
{
	while(1)
	{
		int num_bytes = (int)fread(buffer, 1, sizeof(buffer), stdin);
		if(num_bytes <= 0)
			return 0;
		fwrite(buffer, 1, num_bytes, stdout);
	}
}

/* matches the first line to the string and then passes the rest */
int filter_matchfirst(const char *str)
{
	int len = strlen(str);
	if(fread(buffer, 1, len, stdin) == len)
	{
		if(memcmp(buffer, str, len) == 0)
		{
			/* check for line ending */
			char t = fgetc(stdin);
			if(t == '\n') /* normal LF line ending */
				return filter_pass();
			else if(t == '\r') /* this can be CR or CR/LF */
			{
				t = fgetc(stdin);
				if(t != '\n') /* not a CR/LF */
					fputc(t, stdout);
				return filter_pass();
			}
			else
			{
				/* write out the buffer and the extra character */
				fwrite(buffer, 1, len, stdout);
				fputc(t, stdout);
				return filter_pass();
			}
		}
	}
	
	fwrite(buffer, 1, len, stdout);
	return filter_pass();
}
