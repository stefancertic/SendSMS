#ifndef __ENCODER__H_
#define __ENCODER__H_

int encode_str_gsm7(char *str, char **ret, int encode_unhandled, int *totalbad);
int encode_str_ucs2(char *str, char **ret);


#endif
