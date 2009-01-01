/*
	Small utility that reads from stdin and outputs an
	C table with the data to stdout.
*/
#include <stdio.h>

int main(int argc, char **argv)
{
	int c, f;
	int i = 0;
	FILE *input;
	printf("typedef struct\n");
	printf("{\n");
	printf("\tconst char *filename;\n");
	printf("\tconst char *content;\n");
	printf("} INTERNALFILE;\n");
	printf("\n");
	
	
	for(f = 1; f < argc; f++)
	{
		printf("const char internal_file_%d[] = {", f);
		input = fopen(argv[f], "r");
		
		while(1)
		{
			c = fgetc(input);
			if(feof(input))
				break;
			printf("0x%x, ", c);
			i = (i+1)&0xf;
			if(i == 0)
				printf("\n\t");
		}
		
		fclose(input);
		
		printf("0};\n");
	}

	printf("INTERNALFILE internal_files[] = {");
		
	for(f = 1; f < argc; f++)
	{
		printf("{\"%s\", internal_file_%d },\n", argv[f], f);
	}
	printf("{0}};\n");
	return 0;
}
