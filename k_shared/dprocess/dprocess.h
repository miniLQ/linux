#ifndef _LINUX_DPROCESS_H
#define _LINUX_DPROCESS_H

#ifdef DPROCESS_DEBUG_PRINT
#define dprocess_pr_dbg(fmt, ...) do {pr_err("dprocess: " fmt, ##__VA_ARGS__);} while (0)
#else /* DPROCESS_DEBUG_PRINT */
#define dprocess_pr_dbg(fmt, ...) do {} while (0)
#endif /* DPROCESS_DEBUG_PRINT */

#endif /* _LINUX_DPROCESS_H */

