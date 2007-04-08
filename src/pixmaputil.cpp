#include "pixmaputil.h"

#include <QBitmap>
#include <QImage>

QPixmap PixmapUtil::createTransparentPixmap(int width, int height)
{
#if 1 // the first algorithm appears to be faster, but used to produce tons of X11 errors
	QPixmap pix(width, height);
	QBitmap mask(width, height);
	pix.fill();
	mask.clear();
	pix.setMask(mask);
#else
	QImage img(width, height, QImage::Format_ARGB32);
	img.fill(0x00000000);
	QPixmap pix = QPixmap::fromImage(img);
#endif
	return pix;
}
