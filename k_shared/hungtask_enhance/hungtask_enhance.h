#ifndef _LINUX__HUNGTASK_ENHANCE_H
#define _LINUX__HUNGTASK_ENHANCE_H

#ifdef _HUNGTASK_ENHANCE_DEBUG_PRINT
#define hungtask_enhance_pr_dbg(fmt, ...) do {pr_err("hungtask_enhance: " fmt, ##__VA_ARGS__);} while (0)
#else /* _HUNGTASK_ENHANCE_DEBUG_PRINT */
#define hungtask_enhance_pr_dbg(fmt, ...) do {} while (0)
#endif /* _HUNGTASK_ENHANCE_DEBUG_PRINT */

#endif /* _LINUX__HUNGTASK_ENHANCE_H */

