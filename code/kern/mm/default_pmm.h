#ifndef __KERN_MM_DEFAULT_PMM_H__
#define  __KERN_MM_DEFAULT_PMM_H__

#include <pmm.h>

extern const struct pmm_manager default_pmm_manager;
void first_fit_regression_check(void);

#endif /* ! __KERN_MM_DEFAULT_PMM_H__ */

