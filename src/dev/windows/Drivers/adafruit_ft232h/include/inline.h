
/* inline.h */

#ifndef __INLINE_H__
#define __INLINE_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#include "machdep.h"
#include "ftd2xx.h"

#define APP_CHECK_STATUS(exp){if(exp!=FT_OK){printf("%s:%d:%s(): status(0x%x) \
!= FT_OK\n",__FILE__, __LINE__, __FUNCTION__,exp);exit(1);}else{;}};
#define CHECK_NULL(exp){if(exp==NULL){printf("%s:%d:%s():  NULL expression \
encountered \n",__FILE__, __LINE__, __FUNCTION__);exit(1);}else{;}};



#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
