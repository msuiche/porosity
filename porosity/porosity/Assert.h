/*++

Copyright (c) 2017, Matthieu Suiche

Module Name:
    Assert.h

Abstract:
    Porosity.

Author:
    Matthieu Suiche (m) Feb-2017

Revision History:

--*/
#ifndef _ASSERT_H_
#define _ASSERT_H_

#define assertThrow(_condition, _ExceptionType, _description) \
     printf("ASSERT: %s:%d:%s (%s, %s, %s)\n", __FILE__, __LINE__, __FUNCTION__, #_ExceptionType, _description, #_condition);

#endif