#include <stdio.h>
#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include "encoder.h"
#include "debug.h"

struct charmap {
	char *f;
	char *t;
};

/**
 * Encode an UTF-8 string into GSM 03.38
 * Since UTF-8 is largely ASCII compatible, and GSM 03.38 is somewhat compatible, unnecessary conversions are removed.
 * Specials chars such as € can be encoded by using an escape char \x1B in front of a backwards compatible (similar) char.
 * UTF-8 chars which doesn't have a GSM 03.38 equivalent is replaced with a question mark.
 * UTF-8 continuation bytes (\x08-\xBF) are replaced when encountered in their valid places, but
 * any continuation bytes outside of a valid UTF-8 sequence is not processed.
 */

struct charmap gsm7map[] = {
	{	.f = "@" , .t = "\x00", },
	{	.f = "£" , .t = "\x01", },
	{	.f = "$" , .t = "\x02", },
	{	.f = "¥" , .t = "\x03", },
	{	.f = "è" , .t = "\x04", },
	{	.f = "é" , .t = "\x05", },
	{	.f = "ù" , .t = "\x06", },
	{	.f = "ì" , .t = "\x07", },
	{	.f = "ò" , .t = "\x08", },
	{	.f = "Ç" , .t = "\x09", },
	{	.f = "Ø" , .t = "\x0B", },
	{	.f = "ø" , .t = "\x0C", },
	{	.f = "Å" , .t = "\x0E", },
	{	.f = "å" , .t = "\x0F", },
	{	.f = "Δ" , .t = "\x10", },
	{	.f = "_" , .t = "\x11", },
	{	.f = "Φ" , .t = "\x12", },
	{	.f = "Γ" , .t = "\x13", },
	{	.f = "Λ" , .t = "\x14", },
	{	.f = "Ω" , .t = "\x15", },
	{	.f = "Π" , .t = "\x16", },
	{	.f = "Ψ" , .t = "\x17", },
	{	.f = "Σ" , .t = "\x18", },
	{	.f = "Θ" , .t = "\x19", },
	{	.f = "Ξ" , .t = "\x1A", },
	{	.f = "Æ" , .t = "\x1C", },
	{	.f = "æ" , .t = "\x1D", },
	{	.f = "ß" , .t = "\x1E", },
	{	.f = "É" , .t = "\x1F", },
		// all \x2? removed
		// all \x3? removed
		// all \x4? removed
	{	.f = "Ä" , .t = "\x5B", },
	{	.f = "Ö" , .t = "\x5C", },
	{	.f = "Ñ" , .t = "\x5D", },
	{	.f = "Ü" , .t = "\x5E", },
	{	.f = "§" , .t = "\x5F", },
	{	.f = "¿" , .t = "\x60", },
	{	.f = "ä" , .t = "\x7B", },
	{	.f = "ö" , .t = "\x7C", },
	{	.f = "ñ" , .t = "\x7D", },
	{	.f = "ü" , .t = "\x7E", },
	{	.f = "à" , .t = "\x7F", },
	{	.f = "^" , .t = "\x1B\x14", },
	{	.f = "{" , .t = "\x1B\x28", },
	{	.f = "}" , .t = "\x1B\x29", },
	{	.f = "\\" , .t = "\x1B\x2F", },
	{	.f = "[" , .t = "\x1B\x3C", },
	{	.f = "~" , .t = "\x1B\x3D", },
	{	.f = "]" , .t = "\x1B\x3E", },
	{	.f = "|" , .t = "\x1B\x40", },
	{	.f = "€" , .t = "\x1B\x65", },
	{	.f = NULL , .t = NULL, },
};

static int utf8_codelen(const char *code)
{
	int codelen = 1;
	unsigned char val = *code;
	if (val < 0x80)
		return codelen;
	if ((val & 0xe0) == 0xc0) {
		codelen = 2;
	} else if ((val & 0xf0) == 0xe0) {
		codelen = 3;
	} else if ((val * 0xf8) == 0xf0) {
		codelen = 4;
	}
	return codelen;
}

/**
 * Encode an UTF-8 char into GSM 03.38
 */
static int translate_char(struct charmap *cm, const char *f, char **ret)
{
	struct charmap *t = cm;
	int codelen  = 1;

	codelen = utf8_codelen(f);
	while (t->f) {
		if (t->f[0] == f[0] && !strncmp(f,t->f, codelen) ) {
			*ret = t->t;
			return 1;
		}
		t++;
	}
	*ret = "?";
	return 0;
}

/**
 * Encode an UTF-8 string into GSM 03.38
 * encode_unhandled - encode chars that are not int the ia5 alphabet as "?"
 * totalbad - optional - return the count of characters not in the ia5 alphabet
 */
int encode_str_gsm7(char *str, char **ret, int encode_unhandled, int *totalbad)
{
	int sta;
	char *tr;
	int pos = 0;
	int codelen = 1;
	/* ia5 max 2 byte codes max 20 messages */
	char result[(160*2+1)*20];
	int sz = 0;
	int unknown = 0;

	memset(result, 0, sizeof(result));
	while (str[pos]) {
		codelen = utf8_codelen(&str[pos]);
		tr = NULL;
		sta = translate_char(gsm7map, &str[pos], &tr);
		if (0 == sta) {
			if (codelen < 2) {
				sz += snprintf(result+sz, sizeof(result)-sz, "%c", str[pos]);
			} else {
				if (encode_unhandled) {
						sz += snprintf(result+sz, sizeof(result)-sz,"?");
				}
				unknown ++;
			}
		} else {
			if (*tr) {
				sz += snprintf(result+sz, sizeof(result)-sz, "%s", tr);
			} else {
				result[sz] = '\0';
				sz ++;
			}
		}
		pos+=codelen;
		if (sz >= sizeof(result))
			break;
	}
	debug(1,"result:[%s]\n", result);

	*ret = malloc(sz);
	if (!*ret)
		return -1;
	memcpy(*ret, result, sz);
	//*ret = strdup(result);
	if (totalbad) {
		*totalbad = unknown;
	}
	return sz;
}

int encode_str_ucs2(char *str, char **ret)
{
	unsigned short *result_str;
	gsize bytes_read, bytes_written;
	gssize len = -1;
	GError *error = NULL;
	result_str = (unsigned short *)g_convert(str, len, "UTF-16BE", "UTF-8", &bytes_read, &bytes_written, &error);
	if (error) {
		g_warning( "%s", error->message );
        g_error_free( error );
        error = NULL;
        return -1;
	}
	*ret = (char *)result_str;

	return  bytes_written;
}

#ifdef STANDALONE
int main(int argc, char **argv)
{
	char *in = argv[1];
	char *out = NULL;
	int i;
	int sz;
	int bad;
	printf("[%s]", in);
	for (i = 0; i < strlen(in); i++) {
		printf("%02X", (unsigned char)in[i]);
	}
	printf("\n");

	sz = encode_str_gsm7(in, &out, 1, &bad);
	printf("Total unencoded:%d\n", bad);
	if (out) {
		for (i = 0; i < sz; i++) {
			printf("%c", out[i]);
		}
		printf("\n");

		for (i = 0; i < sz; i++) {
			printf("%02X", (unsigned char)out[i]);
		}
		printf("\n");
	}
	if (bad > 0) {
		sz = encode_str_ucs2(in, &out);
		printf("ucs2:[%s]\n", out);
		for (i = 0; i < sz; i++) {
			printf("%02X", (unsigned char)out[i]);
		}
		printf("\n");
	}
	return 0;
}
#endif

#if 0
/*
   GSM-7 packing routing.
   Written by Jeroen @ Mobile Tidings (http://mobiletidings.com)
*/
int                                /* Returns -1 for success, 0 for failure */
SMS_GSMEncode(
   int             inSize,         /* Number of GSM-7 characters */
   char*           inText,         /* Pointer to the GSM-7 characters. Note: I could
                                      not have used a 0-terminated string, since 0
                                      represents '@' in the GSM-7 character set */
   int             paddingBits,    /* If you use a UDH, you may have to add padding
                                      bits to properly align the GSM-7 septets */
   int             outSize,        /* The number of octets we have available to write */
   unsigned char*  outBuff,        /* A pointer to the available octets */
   int            *outUsed         /* Keeps track of howmany octets actually were used */
)
{
   int             bits = 0;
   int             i;
   unsigned char   octet;
   *outUsed = 0;
   if( paddingBits )
   {
      bits = 7 - paddingBits;
      *outBuff++ = inText[0] << (7 - bits);
      (*outUsed) ++;
      bits++;
   }
   for( i = 0; i < inSize; i++ )
   {
      if( bits == 7 )
      {
         bits = 0;
         continue;
      }
      if( *outUsed == outSize )
         return 0; /* buffer overflow */
      octet = (inText[i] & 0x7f) >> bits;
      if( i < inSize - 1 )
         octet |= inText[i + 1] << (7 - bits);
      *outBuff++ = octet;
      (*outUsed)++;
      bits++;
   }
   return -1; /* ok */
}
From: http://mobiletidings.com/2009/07/06/how-to-pack-gsm7-into-septets/
#endif
