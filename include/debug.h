#ifndef _DEBUG_H
#define _DEBUG_H

#ifndef PDEBUG
#ifdef DEBUG
#define PDEBUG(fmt, ...) do { \
        printf("[%s:%d] "fmt"\n",__FILE__, __LINE__, ##__VA_ARGS__); \
    } while(0);
#else
#define PDEBUG(args...)  
#endif
#endif

#ifndef TDEBUG
#define TDEBUG(fmt, ...) do { \
        printf("[TEST MESSAGE] "fmt"\n",##__VA_ARGS__); \
    } while(0);
#endif

#endif