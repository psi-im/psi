#include "pixmaputil.h"

#include <QBitmap>
#include <QImage>

QPixmap PixmapUtil::createTransparentPixmap(int width, int height)
{
	// Now there's third alternative:
	// QPainter p(&pixmap);
	// p.fillRect(pixmap.rect(), Qt::transparent);
#if 1
	QPixmap pix(width, height);
	pix.fill(Qt::transparent);
/*	QBitmap mask(width, height);
	pix.fill();
	mask.clear();
	pix.setMask(mask);*/
#else
	QImage img(width, height, QImage::Format_ARGB32);
	img.fill(0x00000000);
	QPixmap pix = QPixmap::fromImage(img);
#endif
	return pix;
}
