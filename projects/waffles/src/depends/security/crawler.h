#ifndef CRAWLER_H
#define CRAWLER_H

#include <stdlib.h>

namespace WClasses
{
class WArgReader;
}

unsigned char* downloadFromWeb(const char* szAddr, size_t timeout, size_t* pOutSize);
//void findbrokenlinks(WClasses::WArgReader& args);


#endif
