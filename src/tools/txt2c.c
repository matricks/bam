/*
	Small utility that reads from stdin and outputs an
	C table with the data to stdout.
*/
#include <stdio.h>

int main(int argc, char **argv)
{
	int c;
	int i = 0;
	printf("const char internal_base[] = {");
	while(1)
	{
		c = fgetc(stdin);
		if(feof(stdin))
			break;
		printf("0x%x, ", c);
		i = (i+1)&0xf;
		if(i == 0)
			printf("\n");
	}
	printf("0x00};\n");
	return 0;
}
