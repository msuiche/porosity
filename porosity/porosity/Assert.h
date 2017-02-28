#ifndef _ASSERT_H_
#define _ASSERT_H_

#define assertThrow(_condition, _ExceptionType, _description) \
     printf("ASSERT: %s:%d:%s (%s, %s, %s)\n", __FILE__, __LINE__, __FUNCTION__, #_ExceptionType, _description, #_condition);

#endif