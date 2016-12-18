#ifndef __WEB_H__
#define __WEB_H__


int web_get_uri(const char *line, char **ret);
int web_post_uri(const char *script, const char *line, char **ret);
int web_urlencode(char *buf, const char *text, int text_size, int size);
int web_urldecode(char *text);
int web_init(void);
void web_cleanup(void);

#endif
