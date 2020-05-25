#include<plibc/string.h>

char* strstr(char *str, char *substr)
{
	  while (*str) 
	  {
		    char *Begin = &str[0];
		    char *pattern = &substr[0];
		    
		    // If first character of sub string match, check for whole string
		    while (*str && *pattern && *str == *pattern) 
			{
			      str++;
			      pattern++;
		    }
		    // If complete sub string match, return starting address 
		    if (!*pattern)
		    	  return Begin;
		    	  
		    str = Begin + 1;	// Increament main string 
	  }
	  return NULL;
}