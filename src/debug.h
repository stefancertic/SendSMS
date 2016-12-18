#ifndef __DEBUG__H__

#ifdef DEBUG
#define debug(x, y ...) fprintf(stderr, ## y)
#else
#define debug(x, y ...) do {} while(0)
#endif

#define __DEBUG__H__
#endif
