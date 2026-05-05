#ifndef LIB_H
#define LIB_H

#define NULL  ((void *) 0)
// Esto ya sabes que hace por el nombre
unsigned strlen(const char *s);

// Copies source to destiny and return the quantity of bytes copied
unsigned strCopy(const char *source, char *destiny);

// puts a string in STDIN and jumps a line
void puts2(const char *s);

void reverse(char str[],int len);

// size of string need to be the quantity of digits thats n has (+1 if n is negative) 
void itoa(int n, char* str);

void putInt(int x);


#endif