#ifndef __GIMAGEJPEG_H__
#define __GIMAGEJPEG_H__

#include <WClasses/WImage.h>
#include <stdlib.h>

namespace WClasses
{

void loadJpeg(WImage* pImage, const char* filename);
void saveJpeg(WImage* pImage, const char* filename); //DOEST WORK. LEE WORK ON IT LATER

}

#endif //__GIMAGEJPEG_H__
