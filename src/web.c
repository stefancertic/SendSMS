#include "config.h"
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <curl/curl.h>
#include "debug.h"
#define USERAGENT "SendSMS/" VERSION


static CURL *curl;

struct MemoryStruct {
	char *memory;
	size_t size;
};

static struct MemoryStruct chunk;

static void chunk_init(struct MemoryStruct *chunk)
{
	chunk->memory = NULL;
	chunk->size = 0;
}

static void chunk_free(struct MemoryStruct *chunk)
{
	if (chunk->memory)
		free(chunk->memory);
	chunk->size = 0;
}

static void *myrealloc(void *ptr, size_t size)
{
	/* There might be a realloc() out there that doesn't like reallocing
		NULL pointers, so we take care of it here */
	if (ptr)
		return realloc(ptr, size);
	else
		return malloc(size);
}

static size_t
WriteMemoryCallback(void *ptr, size_t size, size_t nmemb, void *data)
{
	size_t realsize = size * nmemb;
	void *relptr;
	struct MemoryStruct *mem = (struct MemoryStruct *)data;

	relptr = (char *)myrealloc(mem->memory, mem->size + realsize + 1);
	if (relptr) {
		mem->memory = relptr;
	} else {
		return  0;
	}
	if (mem->memory) {
		memcpy(&(mem->memory[mem->size]), ptr, realsize);
		mem->size += realsize;
		mem->memory[mem->size] = 0;
	}
	return realsize;
}

int web_init(void)
{
	curl_global_init(CURL_GLOBAL_ALL);
	curl = curl_easy_init();
	if (!curl)
		return -1;
	return 0;
}

void web_cleanup(void)
{
	curl_easy_cleanup(curl);
}

int web_urldecode(char *text)
{
	char *cp = text;
	char *dp = text;
	int c,n=0;

	while (*cp) {
		if (*cp != '%') {
			*dp = *cp;
			cp ++; dp++;
		} else {
			n = 0;
			sscanf(cp+1,"%2x%n", &c,&n);
			*dp = c; dp++;
			cp+=1+n;
		}
	}
	*dp = '\0';
	return 0;
}

int web_urlencode(char *buf, const char *text, int text_size, int size)
{
	const char *cp;
	int len = 0;
	int pos = 0;

	if (!text_size)
		text_size = strlen(text);

	for (cp = text; pos < text_size ; cp++, pos++)
	{
		if ((*cp >= 'a') && (*cp <= 'z'))
		{
			len++;
			if(len >= (size - 1))
				return -1;
			*buf ++ = *cp;
		}
		else if ((*cp >= 'A') && (*cp <= 'Z'))
		{
			len++;
			if(len >= (size - 1))
				return -1;
			*buf ++ = *cp;
		}
		else if ((*cp >= '0') && (*cp <= '9'))
		{
			len++;
			if(len >= (size - 1))
				return -1;
			*buf ++ = *cp;
		}
		else if (('/' == *cp) || (':' == *cp) ||
			('-' == *cp) || ('.' == *cp) ||
			('_' == *cp) || ('@' == *cp))
		{
			len++;
			if(len >= (size - 1))
				return -1;
			*buf ++ = *cp;
		}
		else
		{
			len += 3;
			if(len >= (size - 1))
				return -1;
			sprintf(buf,"%%%02X",*((unsigned char *)cp));
			buf += 3;
		}
	}
	*buf = 0;
	return 0;
}

int web_get_uri(const char *line, char **ret)
{
	// char urle[4096];
	int rc = 0;
	// int curl_code = 0;
	long http_code = -1;
	// strcpy(urle, line);
	/* specify URL to get */
	curl_easy_setopt(curl, CURLOPT_URL, line);
	/* send all data to this function  */
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
	/* we pass our 'chunk' struct to the callback function */
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
	/* some servers don't like requests that are made without a user-agent
		field, so we provide one */
	curl_easy_setopt(curl, CURLOPT_USERAGENT, USERAGENT);
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, 0);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5);

	chunk_init(&chunk);
	/* get it! */
	/*curl_code = */curl_easy_perform(curl);
	if (!chunk.size) {
		debug(9,"Zero reply for [%.1024s]\n",line);
	}
	curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &http_code);
	if (chunk.memory && chunk.size) {
		// debug(3, "Reply size:[%d]\n", chunk.size);
		*ret = calloc(1,chunk.size+1);
		if (*ret) {
			memcpy(*ret, chunk.memory, chunk.size);
			rc = chunk.size;
		}
	}
	if (http_code != 200) {
		rc = -http_code;

	}
	chunk_free(&chunk);
	return rc;
}

int web_post_uri(const char *script, const char *line, char **ret)
{
	int rc = 0;
	// int curl_code = 0;
	long http_code = -1;
	// strcpy(urle, line);
	// /* specify URL to post */
	curl_easy_setopt(curl, CURLOPT_URL, script);
	/* send all data to this function  */
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
	/* we pass our 'chunk' struct to the callback function */
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
	/* some servers don't like requests that are made without a user-agent
		field, so we provide one */
	curl_easy_setopt(curl, CURLOPT_USERAGENT, USERAGENT);
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, 0);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5);

	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, line);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(line));

	chunk_init(&chunk);
	/* get it! */
	/* curl_code = */ curl_easy_perform(curl);
	if (!chunk.size) {
		debug(9,"Zero reply for [%.1024s]\n",line);
	}
	curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &http_code);
	if (chunk.memory && chunk.size) {
		// debug(3, "Reply size:[%d]\n", chunk.size);
		*ret = calloc(1,chunk.size+1);
		if (*ret) {
			memcpy(*ret, chunk.memory, chunk.size);
			rc = chunk.size;
		}
	}
	if (http_code != 200) {
		rc = -http_code;

	}
	chunk_free(&chunk);
	return rc;
}
