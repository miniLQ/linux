#ifndef _LINUX_KDEMO_H
#define _LINUX_KDEMO_H

#ifdef KDEMO_DEBUG_PRINT
#define kdemo_pr_dbg(fmt, ...) do {pr_err("kdemo: " fmt, ##__VA_ARGS__);} while (0)
#else /* KDEMO_DEBUG_PRINT */
#define kdemo_pr_dbg(fmt, ...) do {} while (0)
#endif /* KDEMO_DEBUG_PRINT */

#endif /* _LINUX_KDEMO_H */

