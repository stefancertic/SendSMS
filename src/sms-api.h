#ifndef __CS_API__
#define __CS_API__

/*
 * HTTP API Error codes:
 * 1 – Unkown error
 * 2 – Syntax error, mandatory parameter missing
 * 10 – Access denied
 * 11 – Invalid message
 * 16 – No message credits left
*/

 #define ERR_UNKNOW			1
 #define ERR_PARAMS			2
 #define ERR_DENIED			10
 #define ERR_INVALID_MSG	11
 #define ERR_NOCREDIT		12

/*
	CHARCODE - Message encoding
	0 (GSM text, default also used for ISO-8859-15 when conversion
			 enabled for account)
	2 (Binary, 8-bit)
	4 (UCS2, Unicode 16-bit)
*/
enum {
	CC_GSM = 0,
	CC_BIN = 2,
	CC_UCS2 = 4,
	CC_AUTO = 65535,
};

enum {
	UDH_UNKNOWN = 0,
	UDH_INTL = 1,
	UDH_ALPHANUM = 5,
};

struct sms_message {
	char *data;
	unsigned int data_len;
	int charcode;
	int dlr;
	int udh;
};

int prepare_sms(int mode, char *message, int dlr, struct sms_message *msg);
int send_prepared_sms(char *username, char *password, int ston, char *sourceaddr, char *destaddr, struct sms_message *msg, char *bulkfile);

int send_sms(char *username, char *password, char *sourceaddr, char *destaddr, int mode, char *message, int dlr);

int send_bulk_sms(char *username, char *password, char *sourceaddr, char *bulkfile, int mode, char *message, int dlr);


#define __CS_API__
#endif
