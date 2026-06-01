#include "../userprog/syscall.h"
#include "lib.h"

unsigned strlen(const char *s) {
	if (s == NULL)
		return 0;
	
	unsigned i = 0;
	for(;s[i] != '\0'; i++);

	return i;
}

unsigned strCopy(const char *source, char *destiny) {
	if (destiny == NULL && source == NULL)
		return 0;
	unsigned i = 0;
	for(; source[i] != '\0'; i++) {
		destiny[i] = source[i];
	}
	destiny[i] = '\0';
	return i;
}

void puts2(const char *s) {
	unsigned len = strlen(s);
	if (len < 1) 
		return;
	Write(s,len,1);
	return;
}

void reverse(char str[],int len) {
	int start = 0;
	int end = len - 1;
	while(start < end) {
		char temp = str[start];
		str[start] = str[end];
		str[end] = temp;
		end--; start++;
	}
}

void itoa(int n, char* str) {
	if (str == NULL)
		return;
	unsigned i = 0;
	
	int isNegative = 0;

	if (n == 0) {
		str[i] = '0';
		i++;
		str[i] = '\0';
	}

	if (n < 0) {
		isNegative = 1;
		n=-n;
	}
	
	int rem;
	while (n != 0) {
    rem = n % 10;
    str[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
    n /= 10;
  }
	
	if (isNegative){
		str[i++] = '-';
	}
	str[i] = '\0';

	reverse(str,i);

}

int atoi2(char *str) {
    int res = 0;
    for (int i = 0; str[i] != '\0'; ++i)
        res = res * 10 + str[i] - '0';
    return res;
}

void putInt(int x) {
	char buff[12]; 
	itoa(x,buff);
	puts2(buff);
}