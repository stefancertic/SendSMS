#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "sms-api.h"
#include "encoder.h"
#include "web.h"
#include "debug.h"

// http://api.cs-networks.net:9011/bin/send? SOURCEADDR=123456&DESTADDR=876543&MESSAGE=TEST&DLR=1

/*
Example sending binary message :
Username: user
Password: pass
Destination address: +447-12-9876541
Message: 0x41 0x42 0x43 0x4a
http://api.cs-networks.net:9011/bin/send?USERNAME=xx&PASSWORD=xx&DESTADDR=xxx&CHARCODE=2&MESSAGE=4142
*/

/*
responce body:

Example: Response indicating success
124365
0
OK

Example: Response indicating failure
-1
2
Recipient (DESTADDR) is missing
*/


/*
GSM default 7-bit alphabet (IA5), max 160 characters
8-bit message, max 140 characters
CS2 (16-bit Unicode), max 70 characters UTF-16BE

1120 bits
*/

int prepare_sms(int mode, char *message, int dlr, struct sms_message *msg)
{
	char *ret = NULL;
	int bad = 0;
	int rc;
	// if udh its data reduces the amount of text
	msg->charcode = mode;
	if (mode == CC_AUTO) {
		rc = encode_str_gsm7(message, &ret, 0, &bad);

		if (bad) {
			if (ret)
				free(ret);
			ret = NULL;
			rc = encode_str_ucs2(message, &ret);
			if (rc < 0) {
				return -1;
			}
			msg->charcode = CC_UCS2;
			msg->data = ret;
			msg->data_len = rc;
		} else {
			msg->charcode = CC_GSM;
			msg->data = ret;
			msg->data_len = rc;
		}
	} else if (mode == CC_GSM) {
		rc = encode_str_gsm7(message, &ret, 1, &bad);
		msg->charcode = CC_GSM;
		msg->data = ret;
		msg->data_len = rc;
	} else if (mode == CC_UCS2) {
			ret = NULL;
			rc = encode_str_ucs2(message, &ret);
			if (rc < 0) {
				return -1;
			}
			msg->charcode = CC_UCS2;
			msg->data = ret;
			msg->data_len = rc;
	} else {
		msg->charcode = CC_BIN;
		msg->data = strdup(message);
		msg->data_len = strlen(message);
	}
	msg->dlr = dlr;
	msg->udh = 0;
	return 0;
}

int process_server_reply(long httpd_code, char *responce, struct sms_message *msg)
{
	char *cp;
	char *strp = responce;
	int line = 0;
	int msgid = -1;
	int status = -1;
	char *errmsg = NULL;
	int rc = -1;

	debug(1,"REPLY:[%s]\n", responce);
	while ((cp = strsep(&strp, "\n\r")) != NULL) {
		if (!*cp) {
			continue;
		}
		// debug(1,"Line:[%s]\n", cp);
		switch (line) {
			case 0:
				msgid = atoi(cp);
				break;
			case 1:
				status = atoi(cp);
				break;
			case 2:
				errmsg = cp;
				break;
			default:
				debug(1, "Unknown API Response: [%s] \n", cp);
		}
		line++;
	}
	if (msgid > 0) {
		fprintf(stdout, "OK: %d (%s)\n", msgid, errmsg);
		rc = 0;
	} else {
		fprintf(stdout, "ERROR: %d (%s)\n", status, errmsg ? : "");
	}
	return rc;
}


#define SRVURL "http://api.cs-networks.net:9011/bin/send"
int send_prepared_sms(char *username, char *password, int ston, char *sourceaddr,
				char *destaddr, struct sms_message *msg, char *bulkfile)
{
	char uname[512];
	char pass[512];
	char msgdata[1024];
	char *req;
	int req_size = 500*1024;
	int sz = 0;
	char *responce = NULL;
	int rc;

	if (!destaddr && !bulkfile) {
		return -1;
	}

	req = calloc(1, req_size);
	if (!req)
		return -1;
	web_urlencode(uname, username, 0, sizeof(uname));
	if (password) {
		web_urlencode(pass, password, 0, sizeof(pass));
	} else {
		*pass = '\0';
	}

	if (msg->charcode == CC_GSM) {
		web_urlencode(msgdata, msg->data, msg->data_len, sizeof(msgdata));
	} else  if (msg->charcode == CC_UCS2) {
		int p = 0;
		unsigned char *dp = (unsigned char *)msg->data;
		sz = 0;
		while (p < msg->data_len) {
			sz += snprintf(msgdata+sz, sizeof(msgdata)-sz, "%02X", dp[p]);
			p++;
		}
	} else if (msg->charcode == CC_BIN) {
			snprintf(msgdata, sizeof(msgdata), "%s", msg->data);
	}
	sz = 0;
	sz += snprintf(req + sz , req_size - sz, "USERNAME=%s&PASSWORD=%s",
		uname, pass);

	if (destaddr) {
		sz += snprintf(req + sz , req_size - sz,"&DESTADDR=%s", destaddr);
	}
	if (sourceaddr && *sourceaddr)
		sz += snprintf(req + sz, req_size - sz, "&SOURCEADDR=%s", sourceaddr);
	else
		sz += snprintf(req + sz, req_size - sz, "&SOURCEADDR=EMPTY");
	if (ston) {
		sz += snprintf(req + sz, req_size - sz, "&SOURCEADDRTON=%d", ston);
	}
	if (msg->dlr)
		sz += snprintf(req + sz, req_size - sz, "&DLR=1");
	if (msg->udh)
		sz += snprintf(req + sz, req_size - sz, "&UDHI=%d", msg->udh);
	if (msg->charcode)
		sz += snprintf(req + sz, req_size - sz, "&CHARCODE=%d", msg->charcode);
	sz += snprintf(req + sz, req_size - sz, "&MESSAGE=%s", msgdata);
	if (bulkfile) {
		char num[512];
		char *cp;
		FILE *fp;
		int start_sz = sz;
		int ll = strlen("&DESTADDR=");
		int nlen;
		fp = fopen(bulkfile, "r");
		if (fp) {
			while ((cp = fgets(num, sizeof(num), fp)) != NULL) {
				if (!*cp)
					continue;
				nlen = strlen(num);
				num[nlen-1] = '\0';
restart:
				if (sz < (req_size - (nlen + ll + 1))) {
					sz += snprintf(req + sz, req_size - sz,"&DESTADDR=%s", num);
				} else {
					rc =  web_post_uri(SRVURL, req, &responce);
					if (responce) {
						rc = process_server_reply(rc, responce, msg);
						free(responce);
						if ( rc != 0)
							break;
					} else if (rc < 0) {
						if (rc == -401) {
							// authenticatipn failed
							break;
						}
					}
					sz = start_sz;
					goto restart;
				}
			}

		} else {
			fprintf(stderr, "Failed to open: [%s]\n", bulkfile);
			return -1;
		}

	}

	debug(10, "request:[%s]\n",req);
	rc =  web_post_uri(SRVURL, req, &responce);
	if (responce) {
		rc = process_server_reply(rc, responce, msg);
		free(responce);
	} else if (rc < 0) {
		if (rc == -401) {
			// authenticatipn failed
		}
	}
	/* Result is -HTTP STATUS CODE */
	free(req);
	return rc;
}


int send_sms(char *username, char *password, char *sourceaddr, char *destaddr,
					int mode, char *message, int dlr)
{
	struct sms_message msg;

	memset(&msg, 0, sizeof(struct sms_message));
	if (prepare_sms(mode, message, dlr, &msg)) {
		return -1;
	}

	return send_prepared_sms(username, password, 0, sourceaddr, destaddr, &msg , NULL);
}

int send_bulk_sms(char *username, char *password, char *sourceaddr, char *bulkfile,
					int mode, char *message, int dlr)
{
	struct sms_message msg;

	memset(&msg, 0, sizeof(struct sms_message));
	if (prepare_sms(mode, message, dlr, &msg)) {
		return -1;
	}

	return send_prepared_sms(username, password, 0, sourceaddr, NULL, &msg , bulkfile);
}
