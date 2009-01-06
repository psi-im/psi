/*
 * Copyright (C) 2007  Barracuda Networks
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "cudaskin.h"

//#define THEME_DEBUG

static QImage myScale(const QImage &in, int newW, int newH)
{
	QImage out(newW, newH, in.format());
	int ow = in.width();
	int oh = in.height();
	int nw = out.width();
	int nh = out.height();
	for(int n2 = 0; n2 < nh; ++n2)
	{
		for(int n = 0; n < nw; ++n)
		{
			QRgb p = in.pixel((ow*n)/nw, (oh*n2)/nh);
			out.setPixel(n, n2, p);
		}
	}
	return out;
}

#ifdef Q_OS_WIN

static QPoint adjusted_position(const QPoint &pos, const QSize &winsize, const QSize &desksize)
{
	QPoint out = pos;

	if(winsize.width() >= desksize.width())
	{
		out.setX(0);
	}
	else
	{
		if(pos.x() + winsize.width() > desksize.width())
			out.setX(desksize.width() - winsize.width());
	}

	if(winsize.height() >= desksize.height())
	{
		out.setY(0);
	}
	else
	{
		if(pos.y() + winsize.height() > desksize.height())
			out.setY(desksize.height() - winsize.height());
	}

	return out;
}

static int cascade_x = 32;
static int cascade_y = 32;

static QPoint get_next_pos(const QSize &winsize, const QSize &desksize)
{
	QPoint pos(cascade_x, cascade_y);

	// calculate next
	cascade_x += 32;
	cascade_y += 32;
	if(cascade_x >= desksize.width() / 2)
		cascade_x = 32;
	if(cascade_y >= desksize.height() / 2)
		cascade_y = 32;

	return adjusted_position(pos, winsize, desksize);
}

#endif

//----------------------------------------------------------------------------
// CudaContainer
//----------------------------------------------------------------------------
// TODO: support different distances from all sides
// TODO: support different justifications for the gradient and overlay
class CudaContainer
{
private:
	QPixmap topLeft, topRight, bottomLeft, bottomRight;
	QImage tlMask, trMask, blMask, brMask;
	QImage topEdge, leftEdge, rightEdge, bottomEdge;
	QImage borderCenter;
	QImage centerGradient;
	QPixmap centerOverlay;
	QColor bgcolor;

public:
	bool setup(const QColor &_bgcolor, const QImage &_gradient, const QImage &_overlay, const QImage &_border, const QImage &_borderMask, int distanceFromEdge, int d2 = -1, int d3 = -1, int d4 = -1)
	{
		bgcolor = _bgcolor;
		centerGradient = _gradient;
		if(!_overlay.isNull())
			centerOverlay = QPixmap::fromImage(_overlay);

		const QImage &img = _border;
		int w = img.width();
		int h = img.height();
		/*QImage tl = img.copy(0, 0, 200, 60);
		QImage tr = img.copy(w - 5, 0, 5, 60);
		QImage bl = img.copy(0, h - 5, 200, 5);
		QImage br = img.copy(w - 5, h - 5, 5, 5);
		QImage te = img.copy(200, 0, w - 200 - 5, 60);
		QImage le = img.copy(0, 60, 200, h - 60 - 5);
		QImage re = img.copy(w - 5, 60, 5, h - 60 - 5);
		QImage be = img.copy(200, h - 5, w - 200 - 5, 5);
		QImage ce = img.copy(200, 60, w - 200 - 5, h - 60 - 5);*/

		int d1 = distanceFromEdge;
		if(d2 == -1)
			d2 = d1;
		if(d3 == -1)
			d3 = d2;
		if(d4 == -1)
			d4 = d3;

		// TODO: sanity check distances

		QImage tl = img.copy(0, 0, d4, d1);
		QImage tr = img.copy(w - d2, 0, d2, d1);
		QImage bl = img.copy(0, h - d3, d4, d3);
		QImage br = img.copy(w - d2, h - d3, d2, d3);
		QImage te = img.copy(d4, 0, w - d4 - d2, d1);
		QImage le = img.copy(0, d1, d4, h - d3 - d1);
		QImage re = img.copy(w - d2, d1, d2, h - d3 - d1);
		QImage be = img.copy(d4, h - d3, w - d4 - d2, d3);
		QImage bc = img.copy(d4, d1, w - d4 - d2, h - d3 - d1);

		if(!_borderMask.isNull())
		{
			QImage m = _borderMask.convertToFormat(QImage::Format_MonoLSB);
			tlMask = m.copy(0, 0, d4, d1);
			trMask = m.copy(w - d2, 0, d2, d1);
			blMask = m.copy(0, h - d3, d4, d3);
			brMask = m.copy(w - d2, h - d3, d2, d3);
		}

		/*QImage tl = img.copy(0, 0, p1.x(), p1.y());
		QImage tr = img.copy(p2.x(), 0, w - p2.x(), p2.y());
		QImage bl = img.copy(0, p3.y(), p3.x(), h - p3.y());
		QImage br = img.copy(p4.x(), p4.y(), w - p4.x(), h - p4.y());*/

		topLeft = QPixmap::fromImage(tl);
		topRight = QPixmap::fromImage(tr);
		bottomLeft = QPixmap::fromImage(bl);
		bottomRight = QPixmap::fromImage(br);

		topEdge = te;
		leftEdge = le;
		rightEdge = re;
		bottomEdge = be;

		if(!bgcolor.isValid())
			borderCenter = bc;

		/*topEdge = QPixmap::fromImage(te);
		leftEdge = QPixmap::fromImage(le);
		rightEdge = QPixmap::fromImage(re);
		bottomEdge = QPixmap::fromImage(be);
		center = QPixmap::fromImage(ce);*/

		return true;
	}

	void draw(QPainter *p, int w, int h) const
	{
		if(bgcolor.isValid())
			p->fillRect(0, 0, w, h, bgcolor);

		if(!centerGradient.isNull())
		{
			QImage cg = centerGradient.scaled(w, centerGradient.height());
			p->drawImage(0, 0, cg);
		}
		if(!centerOverlay.isNull())
			p->drawPixmap(0, 0, centerOverlay);

		p->drawPixmap(0, 0, topLeft);
		p->drawPixmap(w - topRight.width(), 0, topRight);
		p->drawPixmap(0, h - bottomLeft.height(), bottomLeft);
		p->drawPixmap(w - bottomRight.width(), h - bottomRight.height(), bottomRight);

		int ew, eh;
		ew = w - topLeft.width() - topRight.width();
		eh = h - topLeft.height() - bottomLeft.height();
		//QImage te = topEdge.scaled(ew, topEdge.height());
		//QImage be = bottomEdge.scaled(ew, bottomEdge.height());
		//QImage le = leftEdge.scaled(leftEdge.width(), eh);
		//QImage re = rightEdge.scaled(rightEdge.width(), eh);
		QImage te = myScale(topEdge, ew, topEdge.height());
		QImage be = myScale(bottomEdge, ew, bottomEdge.height());
		QImage le = myScale(leftEdge, leftEdge.width(), eh);
		QImage re = myScale(rightEdge, rightEdge.width(), eh);
		p->drawImage(topLeft.width(), 0, te);
		p->drawImage(bottomLeft.width(), h - bottomEdge.height(), be);
		p->drawImage(0, topLeft.height(), le);
		p->drawImage(w - rightEdge.width(), topRight.height(), re);
		if(!borderCenter.isNull())
		{
			//QImage be = borderCenter.scaled(ew, eh);
			QImage be = myScale(borderCenter, ew, eh);
			p->drawImage(topLeft.width(), topLeft.height(), be);
		}

		//QImage ce = center.toImage().scaled(ew, eh);
		//p->drawImage(topLeft.width(), topLeft.height(), ce);

		/*p->fillRect(0, 0, width(), height(), Qt::black);

		p->drawPixmap(0, 0, topLeft);
		p->drawPixmap(width() - topRight.width(), 0, topRight);
		p->drawPixmap(0, height() - bottomLeft.height(), bottomLeft);
		p->drawPixmap(width() - bottomRight.width(), height() - bottomRight.height(), bottomRight);

		int ew, eh;
		ew = width() - topLeft.width() - topRight.width();
		eh = height() - topLeft.height() - bottomLeft.height();
		QImage te = topEdge.toImage().scaled(ew, topEdge.height());
		QImage be = bottomEdge.toImage().scaled(ew, bottomEdge.height());
		QImage le = leftEdge.toImage().scaled(leftEdge.width(), eh);
		QImage re = rightEdge.toImage().scaled(rightEdge.width(), eh);
		p->drawImage(topLeft.width(), 0, te);
		p->drawImage(bottomLeft.width(), height() - bottomEdge.height(), be);
		p->drawImage(0, topLeft.height(), le);
		p->drawImage(width() - rightEdge.width(), topRight.height(), re);

		QImage ce = center.toImage().scaled(ew, eh);
		p->drawImage(topLeft.width(), topLeft.height(), ce);*/
	}

	QBitmap mask(int width, int height) const
	{
		QImage out(width, height, QImage::Format_MonoLSB);
		out.fill(Qt::color0);
		applyImage(0, 0, tlMask, &out);
		applyImage(width - trMask.width(), 0, trMask, &out);
		applyImage(0, height - blMask.height(), blMask, &out);
		applyImage(width - brMask.width(), height - brMask.height(), brMask, &out);
		return QBitmap::fromImage(out);
	}

	void applyImage(int x, int y, const QImage &a, QImage *b) const
	{
		for(int ny = 0; ny < a.height(); ++ny)
		{
			for(int nx = 0; nx < a.width(); ++nx)
			{
				b->setPixel(x + nx, y + ny, a.pixelIndex(nx, ny));
			}
		}
	}
};

//----------------------------------------------------------------------------
// CudaSkin
//----------------------------------------------------------------------------
class CudaSkin
{
public:
	QImage windowGradient, windowWatermark, windowBorder, windowBorderMask;
	QImage buttonUp, buttonDown;
	QImage buttonClose, buttonMax, buttonMin;
	QImage panel;

	int borderNumber1;
	int panelNumber1, panelNumber2, panelNumber3, panelNumber4;

	QImage tabFore, tabBack;

	QColor menubarBg;
	QColor frameBg;
	QColor frameCol1, frameCol2;
	QColor buttonText, windowText, base, highlight, highlightedText;
	QColor selectBg, contactOnline, expansionLine, expansionText;

	QPixmap button_icon_im, button_icon_file, button_up;

	QImage defaultAvatar;

	CudaContainer ccTabFore, ccTabBack;

	CudaTabDlgDelegate *tabdeleg;
};

static CudaSkin *g_skin = 0;

bool cuda_loadTheme(const QString &path)
{
	if(g_skin)
		return false;

	QImage windowGradient(path + "/Barracuda_IM_Window_Gradient.png");
	if(windowGradient.isNull())
		return false;
	QImage windowWatermark(path + "/Barracuda_IM_Window_Watermark.png");
	if(windowWatermark.isNull())
		return false;
	QImage windowBorder(path + "/Barracuda_IM_Window_Border.png");
	if(windowBorder.isNull())
		return false;
	QImage windowBorderMask(path + "/Barracuda_IM_Window_Border_Mask.png");
	if(windowBorderMask.isNull())
		return false;
	QImage buttonUp(path + "/Barracuda_IM_Button_Up.png");
	if(buttonUp.isNull())
		return false;
	QImage buttonDown(path + "/Barracuda_IM_Button_Down.png");
	if(buttonDown.isNull())
		return false;
	QImage buttonClose(path + "/Barracuda_IM_Window_Control_Close.png");
	if(buttonClose.isNull())
		return false;
	QImage buttonMax(path + "/Barracuda_IM_Window_Control_Max.png");
	if(buttonMax.isNull())
		return false;
	QImage buttonMin(path + "/Barracuda_IM_Window_Control_Min.png");
	if(buttonMin.isNull())
		return false;
	QImage panel(path + "/Barracuda_IM_Panel.png");
	if(panel.isNull())
		return false;

	QImage tabFore(path + "/Barracuda_IM_Tab_Fore.png");
	if(tabFore.isNull())
		return false;
	QImage tabBack(path + "/Barracuda_IM_Tab_Back.png");
	if(tabBack.isNull())
		return false;

	QPixmap button_icon_im(path + "/Barracuda_IM_Button_Icon_IM.png");
	if(button_icon_im.isNull())
		return false;
	QPixmap button_icon_file(path + "/Barracuda_IM_Button_Icon_Send_File.png");
	if(button_icon_file.isNull())
		return false;
	QPixmap button_up(path + "/Barracuda_IM_Button_Up.png");
	if(button_up.isNull())
		return false;

	QImage defaultAvatar(path + "/defaultAvatar.png");
	if(defaultAvatar.isNull())
		return false;

	/*buttonClose.setAlphaChannel(QImage());
	buttonMax.setAlphaChannel(QImage());
	buttonMin.setAlphaChannel(QImage());*/

	g_skin = new CudaSkin;

	g_skin->windowGradient = windowGradient;
	g_skin->windowWatermark = windowWatermark;
	g_skin->windowBorder = windowBorder;
	g_skin->windowBorderMask = windowBorderMask;
	g_skin->buttonUp = buttonUp;
	g_skin->buttonDown = buttonDown;
	g_skin->buttonClose = buttonClose;
	g_skin->buttonMax = buttonMax;
	g_skin->buttonMin = buttonMin;
	g_skin->panel = panel;
	g_skin->button_icon_im = button_icon_im;
	g_skin->button_icon_file = button_icon_file;
	g_skin->button_up = button_up;
	g_skin->defaultAvatar = defaultAvatar;

	g_skin->borderNumber1 = 5;
	g_skin->panelNumber1 = 18;
	g_skin->panelNumber2 = 6;
	g_skin->panelNumber3 = 5;
	g_skin->panelNumber4 = 6;

	g_skin->tabFore = tabFore;
	g_skin->tabBack = tabBack;

	g_skin->menubarBg = QColor(0, 70, 153, 0x80);
	g_skin->frameBg = QColor("#D9F6FF");
	g_skin->frameCol1 = QColor(162, 205, 231);
	g_skin->frameCol2 = Qt::white;

	g_skin->buttonText = Qt::white;
	g_skin->windowText = QColor(0, 70, 153);
	g_skin->base = Qt::white;
	g_skin->highlight = QColor(0, 161, 231);
	g_skin->highlightedText = Qt::white;
	g_skin->selectBg = QColor(213, 247, 255);
	g_skin->contactOnline = QColor(21, 107, 208);
	g_skin->expansionLine = QColor(181, 221, 240);
	g_skin->expansionText = QColor(82, 184, 255);

	g_skin->ccTabFore.setup(QColor(), QImage(), QImage(), g_skin->tabFore, QImage(), 5);
	g_skin->ccTabBack.setup(QColor(), QImage(), QImage(), g_skin->tabBack, QImage(), 6);

	g_skin->tabdeleg = new CudaTabDlgDelegate;

	return true;
}

bool cuda_isThemed()
{
	return (g_skin ? true : false);
}

void cuda_unloadTheme()
{
	delete g_skin->tabdeleg;

	delete g_skin;
	g_skin = 0;
}

static void myDrawRoundRect(QPainter *p, const QRect &rect)
{
	int x = rect.x();
	int y = rect.y();
	int w = rect.width();
	int h = rect.height();
	p->drawRect(x + 2, y, w - 5, h);
	p->drawLine(x + 1, y + 1, x + 1, y + 1 + h - 2);
	p->drawLine(x + w - 2, y + 1, x + w - 2, y + 1 + h - 2);
	p->drawLine(x + 0, y + 2, x + 0, y + 2 + h - 4);
	p->drawLine(x + w - 1, y + 2, x + w - 1, y + 2 + h - 4);
}

//----------------------------------------------------------------------------
// CudaStyle
//----------------------------------------------------------------------------
CudaStyle::CudaStyle()
{
}

void CudaStyle::drawControl(ControlElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
	if(element == CE_PushButton)
	{
#ifdef THEME_DEBUG
		printf("control element: CE_PushButton\n");
#endif
		QWindowsStyle::drawControl(element, option, painter, widget);
	}
	else if(element == CE_PushButtonBevel)
	{
		QImage i;
		if(option->state & State_Sunken)
			i = g_skin->buttonDown.scaled(option->rect.width(), option->rect.height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
		else
			i = g_skin->buttonUp.scaled(option->rect.width(), option->rect.height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
		painter->drawImage(option->rect.x(), option->rect.y(), i);

		if(const QStyleOptionButton *btn = qstyleoption_cast<const QStyleOptionButton *>(option))
		{
			if(btn->features & QStyleOptionButton::HasMenu)
			{
				int mbi = pixelMetric(PM_MenuButtonIndicator, btn, widget);
				QRect ir = btn->rect;
				QStyleOptionButton newBtn = *btn;
				newBtn.rect = QRect(ir.right() - mbi, ir.height() - 20, mbi, ir. height() - 4);
				drawPrimitive(PE_IndicatorArrowDown, &newBtn, painter, widget);
			}
		}

#ifdef THEME_DEBUG
		printf("control element: CE_PushButtonBevel\n");
#endif
		//QWindowsStyle::drawControl(element, option, painter, widget);
	}
	else if(element == CE_PushButtonLabel)
	{
#ifdef THEME_DEBUG
		printf("control element: CE_PushButtonLabel\n");
#endif
		QWindowsStyle::drawControl(element, option, painter, widget);
	}
	else if(element == CE_ToolButtonLabel)
	{
#ifdef THEME_DEBUG
		printf("control element: CE_ToolButtonLabel\n");
#endif
		QWindowsStyle::drawControl(element, option, painter, widget);
	}
#if !defined(Q_OS_MAC)
	else if(element == CE_MenuBarItem)
	{
		//painter->translate(QPoint(0, 4));
#ifdef THEME_DEBUG
		printf("control element: CE_MenuBarItem\n");
#endif
		if((option->state & State_Selected) && (option->state & State_Enabled))
		{
			painter->fillRect(option->rect, g_skin->menubarBg);
			painter->save();
			painter->setPen(Qt::white);
			painter->setBrush(Qt::white);
			QRect rect = option->rect;
			rect.setHeight(rect.height() - 2);
			//painter->drawRect(rect);
			myDrawRoundRect(painter, rect);
			painter->restore();
			//painter->fillRect(option->rect, Qt::white);
			//QWindowsStyle::drawControl(element, option, painter, widget);
			const QStyleOptionMenuItem *mbi = qstyleoption_cast<const QStyleOptionMenuItem *>(option);
			bool down = mbi->state & State_Sunken;
			uint alignment = Qt::AlignCenter | Qt::TextShowMnemonic | Qt::TextDontClip | Qt::TextSingleLine;
			QPalette pal = option->palette;
			pal.setColor(QPalette::ButtonText, QColor(0, 70, 153));
			rect = option->rect;
			if(down)
				rect.translate(pixelMetric(PM_ButtonShiftHorizontal, mbi, widget), pixelMetric(PM_ButtonShiftVertical, mbi, widget));
			drawItemText(painter, rect, alignment, pal, option->state & State_Enabled, mbi->text, QPalette::ButtonText);
			return;
		}
		else
			painter->fillRect(option->rect, g_skin->menubarBg);

		QCommonStyle::drawControl(element, option, painter, widget);
		//painter->restore();
	}
	else if(element == CE_MenuBarEmptyArea)
	{
#ifdef THEME_DEBUG
		printf("control element: CE_MenuBarEmptyArea\n");
#endif
		painter->fillRect(option->rect, g_skin->menubarBg);
		//QCommonStyle::drawControl(element, option, painter, widget);
	}
#endif
	else if(element == CE_TabBarTab)
	{
#ifdef THEME_DEBUG
		printf("control element: CE_TabBarTab\n");
#endif
		QWindowsStyle::drawControl(element, option, painter, widget);
	}
	else if(element == CE_TabBarTabShape)
	{
#ifdef THEME_DEBUG
		printf("control element: CE_TabBarTabShape\n");
#endif
		if(const QStyleOptionTab *tab = qstyleoption_cast<const QStyleOptionTab *>(option))
		{
			painter->save();

			QRect rect(tab->rect);
			bool selected = tab->state & State_Selected;

			//QWindowsStyle::drawControl(element, option, painter, widget);
			painter->translate(rect.x(), rect.y());
			if(selected)
				g_skin->ccTabFore.draw(painter, rect.width(), rect.height());
			else
				g_skin->ccTabBack.draw(painter, rect.width(), rect.height());

			painter->restore();
		}
	}
	else if(element == CE_TabBarTabLabel)
	{
#ifdef THEME_DEBUG
		printf("control element: CE_TabBarTabLabel\n");
#endif
		if(const QStyleOptionTab *_tab = qstyleoption_cast<const QStyleOptionTab *>(option))
		{
			QStyleOptionTab tab(*_tab);
			QColor col = tab.palette.color(QPalette::WindowText);
			if(col != QColor(Qt::darkGreen) && col != QColor(Qt::red))
				tab.palette.setColor(QPalette::WindowText, Qt::white);
			QWindowsStyle::drawControl(element, &tab, painter, widget);
		}
	}
	else
	{
#ifdef THEME_DEBUG
		printf("control element: %d\n", element);
#endif
		QWindowsStyle::drawControl(element, option, painter, widget);
	}
}

void CudaStyle::drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
	if(element == PE_PanelLineEdit)
	{
#ifdef THEME_DEBUG
		printf("primitive element: PE_PanelLineEdit\n");
#endif
		QWindowsStyle::drawPrimitive(element, option, painter, widget);
	}
	else if(element == PE_PanelButtonTool)
	{
		QImage i;
		if(option->state & State_Sunken)
			i = g_skin->buttonDown.scaled(option->rect.width(), option->rect.height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
		else
			i = g_skin->buttonUp.scaled(option->rect.width(), option->rect.height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
		painter->drawImage(option->rect.x(), option->rect.y(), i);
#ifdef THEME_DEBUG
		printf("control element: PE_PanelButtonTool\n");
#endif
		//QWindowsStyle::drawPrimitive(element, option, painter, widget);
	}
	else if(element == PE_FrameLineEdit)
	{
#ifdef THEME_DEBUG
		printf("primitive element: PE_FrameLineEdit\n");
#endif
		QWindowsStyle::drawPrimitive(element, option, painter, widget);
	}
	else if(element == PE_Frame)
	{
#ifdef THEME_DEBUG
		printf("primitive element: PE_Frame\n");
#endif
		painter->save();
		painter->setPen(g_skin->frameCol1);
		//painter->drawRect(option->rect);
		painter->drawRect(option->rect.x(), option->rect.y(), option->rect.width() - 1, option->rect.height() - 1);
		painter->setPen(g_skin->frameCol2);
		painter->drawRect(option->rect.x() + 1, option->rect.y() + 1, option->rect.width() - 3, option->rect.height() - 3);
		painter->restore();
		//QWindowsStyle::drawPrimitive(element, option, painter, widget);
	}
	else if(element == PE_PanelButtonCommand)
	{
#ifdef THEME_DEBUG
		printf("primitive element: PE_PanelButtonCommand\n");
#endif
		QWindowsStyle::drawPrimitive(element, option, painter, widget);
	}
	else if(element == PE_IndicatorArrowDown)
	{
#ifdef THEME_DEBUG
		printf("primitive element: PE_IndicatorArrowDown\n");
#endif
		QWindowsStyle::drawPrimitive(element, option, painter, widget);
	}
	else if(element == PE_FrameFocusRect)
	{
#ifdef THEME_DEBUG
		printf("primitive element: PE_FrameFocusRect\n");
#endif
		QWindowsStyle::drawPrimitive(element, option, painter, widget);
	}
	else if(element == PE_FrameTabWidget)
	{
#ifdef THEME_DEBUG
		printf("primitive element: PE_FrameTabWidget\n");
#endif
		QWindowsStyle::drawPrimitive(element, option, painter, widget);
	}
	else if(element == PE_FrameTabBarBase)
	{
#ifdef THEME_DEBUG
		printf("primitive element: PE_TabBarBase\n");
#endif
		// don't draw
		//QWindowsStyle::drawPrimitive(element, option, painter, widget);
	}
	else
	{
#ifdef THEME_DEBUG
		printf("primitive element: %d\n", element);
#endif
		QWindowsStyle::drawPrimitive(element, option, painter, widget);
	}
}

QSize CudaStyle::sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &size, const QWidget *widget) const
{
	QSize newSize = QWindowsStyle::sizeFromContents(type, option, size, widget);
	if(type == CT_MenuBarItem)
	{
		newSize.setHeight(newSize.height() + 2);
	}
	return newSize;
}

int CudaStyle::pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const
{
	if(metric == PM_MenuBarItemSpacing)
		return 3;
	else if(metric == PM_MenuBarVMargin)
		return 2;
	else if(metric == PM_MenuBarHMargin)
		return 1;
	else if(metric == PM_MenuBarPanelWidth)
		return 0; // ###
	//else if(metric == PM_TabBarTabHSpace)
	//	return 7;
	else
		return QWindowsStyle::pixelMetric(metric, option, widget);
}

QPalette CudaStyle::standardPalette() const
{
	QPalette pal;
	pal.setColor(QPalette::ButtonText, g_skin->buttonText);
	pal.setColor(QPalette::WindowText, g_skin->windowText);
	pal.setColor(QPalette::Base, g_skin->base);
	pal.setColor(QPalette::Highlight, g_skin->highlight);
	pal.setColor(QPalette::HighlightedText, g_skin->highlightedText);
	return pal;
}

//----------------------------------------------------------------------------
// CudaFrame
//----------------------------------------------------------------------------
// FIXME: this is probably wrong when stuff gets small
static int getResizeZone(int w, int h, int x, int y)
{
	QPoint at(x, y);

	QRect top(20, 0, w - 40, 5);
	QRect bottom(20, h - 5, w - 40, 5);
	QRect left(0, 20, 5, h - 40);
	QRect right(w - 5, 20, 5, h - 40);
	QRect tl1(0, 0, 20, 5);
	QRect tl2(0, 0, 5, 20);
	QRect tr1(w - 20, 0, 20, 5);
	QRect tr2(w - 5, 0, 5, 20);
	QRect bl1(0, h - 5, 20, 5);
	QRect bl2(0, h - 20, 5, 20);
	QRect br1(w - 20, h - 5, 20, 5);
	QRect br2(w - 5, h - 20, 5, 20);

	if(tl1.contains(at) || tl2.contains(at))
		return 0;
	else if(top.contains(at))
		return 1;
	else if(tr1.contains(at) || tr2.contains(at))
		return 2;
	else if(left.contains(at))
		return 3;
	else if(right.contains(at))
		return 4;
	else if(bl1.contains(at) || bl2.contains(at))
		return 5;
	else if(bottom.contains(at))
		return 6;
	else if(br1.contains(at) || br2.contains(at))
		return 7;
	else
		return -1;
}

class CudaFrame::Private : public QObject
{
	Q_OBJECT
public:
	CudaFrame *q;

	QRect minRect, maxRect, closeRect;
	int dragZone;
	bool moving;
	QPoint resizeStartAt;
	QRect resizeStartRect;
	QTimer cursorTimer;

	Private(CudaFrame *_q) :
		QObject(_q),
		q(_q),
		moving(false),
		cursorTimer(this)
	{
		connect(&cursorTimer, SIGNAL(timeout()), SLOT(cursorCheck()));
	}

private slots:
	void cursorCheck()
	{
		QPoint at = q->mapFromGlobal(QCursor::pos());

		// cursor display
		int zone = getResizeZone(q->width(), q->height(), at.x(), at.y());

		if(zone == -1 && !q->resizing)
		{
			q->setCursor(Qt::ArrowCursor);
			cursorTimer.stop();
		}
	}
};

CudaFrame::CudaFrame(QWidget *parent) : QFrame(parent, Qt::FramelessWindowHint)
{
	d = new Private(this);
	resizing = false;
	d->moving = false;
	container = new CudaContainer;
	setMouseTracking(true);
	setup();
}

CudaFrame::~CudaFrame()
{
	delete d;
}

void CudaFrame::setup()
{
	container->setup(g_skin->frameBg, g_skin->windowGradient, g_skin->windowWatermark, g_skin->windowBorder, g_skin->windowBorderMask, g_skin->borderNumber1);

	setContentsMargins(8, 29, 8, 8);
}

void CudaFrame::paintEvent(QPaintEvent *)
{
#ifdef THEME_DEBUG
	printf("CudaFrame::paintEvent\n");
#endif

	QPainter paint(this);
	container->draw(&paint, width(), height());

	paint.drawPixmap(8, 8, windowIcon().pixmap(16, 16));

	QFont f = font();
	f.setBold(true);
	//f.setPixelSize(10);
	QFontMetrics fm(f);
	paint.setPen(Qt::white);
	paint.setFont(f);

	paint.drawText(32, 8 + 16 - 4, windowTitle());

	// TODO: convert to RGB?
	QImage bc = g_skin->buttonClose;
	bc.setAlphaChannel(makeAlpha(bc, 65));
	QImage bmax = g_skin->buttonMax;
	bmax.setAlphaChannel(makeAlpha(bmax, 65));
	QImage bmin = g_skin->buttonMin;
	bmin.setAlphaChannel(makeAlpha(bmin, 65));
	int at = width() - 8 - bc.width();
	paint.drawImage(at, 11, bc);
	d->closeRect = QRect(at, 11, bc.width(), bc.height());
	at -= bmax.width() + 5;
	paint.drawImage(at, 11, bmax);
	d->maxRect = QRect(at, 11, bmax.width(), bmax.height());
	at -= bmin.width() + 5;
	paint.drawImage(at, 11, bmin);
	d->minRect = QRect(at, 11, bmin.width(), bmin.height());
}

QImage CudaFrame::makeAlpha(const QImage &in, int percent)
{
	QImage out = in.alphaChannel();
	for(int y = 0; y < out.height(); ++y)
	{
		for(int x = 0; x < out.width(); ++x)
		{
			int alpha = out.pixelIndex(x, y);
			alpha = 255 * percent / 100;
			out.setPixel(x, y, alpha);
		}
	}
	return out;
}

void CudaFrame::resizeEvent(QResizeEvent *)
{
	// TODO: set at start as well
	setMask(container->mask(width(), height()));
}

void CudaFrame::mousePressEvent(QMouseEvent *event)
{
	if(event->button() == Qt::LeftButton)
	{
		dragPosition = event->globalPos() - frameGeometry().topLeft();

		QPoint at = mapFromGlobal(event->globalPos());
#ifdef THEME_DEBUG
		printf("at: (%d,%d), size: %dx%d\n", at.x(), at.y(), width(), height());
#endif

		if(d->closeRect.contains(at))
		{
			event->accept();
			close();
			return;
		}
		else if(d->maxRect.contains(at))
		{
			event->accept();
			if(!isMaximized())
				showMaximized();
			else
				showNormal();
			return;
		}
		else if(d->minRect.contains(at))
		{
			event->accept();
			showMinimized();
			return;
		}

		if(!isMaximized())
		{
			int zone = getResizeZone(width(), height(), at.x(), at.y());

			if(zone != -1)
			{
				d->dragZone = zone;
				d->resizeStartAt = event->globalPos();
				d->resizeStartRect = geometry();
				resizing = true;
			}
			else
				d->moving = true;
		}

		event->accept();
	}
}

void CudaFrame::mouseMoveEvent(QMouseEvent *event)
{
	if(event->buttons() & Qt::LeftButton)
	{
		if(resizing)
		{
			QPoint at = event->globalPos();
			int dragZone = d->dragZone;

			//int cx = at.x() - dragPosition.x();
			//int cy = at.y() - dragPosition.y();

			int cx = at.x() - d->resizeStartAt.x();
			int cy = at.y() - d->resizeStartAt.y();

			// ignore x-axis for top/bottom sizing
			if(dragZone == 1 || dragZone == 6)
				cx = 0;
			// ignore y-axis for left/right sizing
			else if(dragZone == 3 || dragZone == 4)
				cy = 0;

			bool movex = false;
			bool movey = false;

			if((dragZone == 0 || dragZone == 1 || dragZone == 2) && cy != 0)
				movey = true;
			if((dragZone == 0 || dragZone == 3 || dragZone == 5) && cx != 0)
				movex = true;

			int mx = 0;
			int my = 0;

			if(movex)
			{
				mx = cx;
				cx = -cx;
			}

			if(movey)
			{
				my = cy;
				cy = -cy;
			}

			int newX = d->resizeStartRect.x() + mx;
			int newY = d->resizeStartRect.y() + my;
			int newWidth = d->resizeStartRect.width() + cx;
			int newHeight = d->resizeStartRect.height() + cy;
			if(newWidth < minimumWidth())
			{
				if(movex)
					newX = d->resizeStartRect.x() + (d->resizeStartRect.width() - minimumWidth());
				else
					newX = d->resizeStartRect.x();
				newWidth = minimumWidth();
			}
			if(newHeight < minimumHeight())
			{
				if(movey)
					newY = d->resizeStartRect.y() + (d->resizeStartRect.height() - minimumHeight());
				else
					newY = d->resizeStartRect.y();
				newHeight = minimumHeight();
			}

			setGeometry(newX, newY, newWidth, newHeight);

			/*if(mx != 0 || my != 0)
				move(x() + mx, y() + my);

			if(cx != 0 || cy != 0)
				resize(width() + cx, height() + cy);*/

			//dragPosition = at;
		}
		else if(d->moving)
			move(event->globalPos() - dragPosition);

		event->accept();
	}

	if(!resizing && !d->moving && !isMaximized())
	{
		QPoint at = mapFromGlobal(event->globalPos());

		// cursor display
		int zone = getResizeZone(width(), height(), at.x(), at.y());

		if(zone == 0 || zone == 7)
			setCursor(Qt::SizeFDiagCursor);
		else if(zone == 1 || zone == 6)
			setCursor(Qt::SizeVerCursor);
		else if(zone == 2 || zone == 5)
			setCursor(Qt::SizeBDiagCursor);
		else if(zone == 3 || zone == 4)
			setCursor(Qt::SizeHorCursor);
		else
			setCursor(Qt::ArrowCursor);

		if(zone != -1)
			d->cursorTimer.start(100);
	}
}

void CudaFrame::mouseReleaseEvent(QMouseEvent *)
{
	resizing = false;
	d->moving = false;
}

bool CudaFrame::event(QEvent *e)
{
	if(e->type() == QEvent::WindowTitleChange || e->type() == QEvent::WindowIconChange)
		update();
	return QFrame::event(e);
}

//----------------------------------------------------------------------------
// CudaTabDlgDelegate
//----------------------------------------------------------------------------
class TabDlgState : public QObject
{
	Q_OBJECT
public:
	QWidget *widget;
	CudaContainer *container;
	QPoint dragPosition;
	bool resizing;
	QRect minRect, maxRect, closeRect;
	int dragZone;
	bool moving;
	QPoint resizeStartAt;
	QRect resizeStartRect;
	QTimer cursorTimer;

	// FIXME: set qobject parent?
	TabDlgState(QWidget *w) :
		moving(false),
		cursorTimer(this)
	{
		connect(&cursorTimer, SIGNAL(timeout()), SLOT(cursorCheck()));

		resizing = false;
		moving = false;
		container = new CudaContainer;
		widget = w;
		widget->setMouseTracking(true);
		container->setup(g_skin->frameBg, g_skin->windowGradient, g_skin->windowWatermark, g_skin->windowBorder, g_skin->windowBorderMask, g_skin->borderNumber1);
		widget->setContentsMargins(8, 29, 8, 8);
		cuda_applyTheme(widget);
	}

	~TabDlgState()
	{
	}

private slots:
	void cursorCheck()
	{
		QPoint at = widget->mapFromGlobal(QCursor::pos());

		// cursor display
		int zone = getResizeZone(widget->width(), widget->height(), at.x(), at.y());

		if(zone == -1 && !resizing)
		{
			widget->setCursor(Qt::ArrowCursor);
			cursorTimer.stop();
		}
	}

public:
	QImage makeAlpha(const QImage &in, int percent)
	{
		QImage out = in.alphaChannel();
		for(int y = 0; y < out.height(); ++y)
		{
			for(int x = 0; x < out.width(); ++x)
			{
				int alpha = out.pixelIndex(x, y);
				alpha = 255 * percent / 100;
				out.setPixel(x, y, alpha);
			}
		}
		return out;
	}
};

class CudaTabDlgDelegate::Private
{
public:
	CudaTabDlgDelegate *q;

	QHash<QWidget*,TabDlgState*> state;

	Private(CudaTabDlgDelegate *_q) :
		q(_q)
	{
	}
};

CudaTabDlgDelegate::CudaTabDlgDelegate()
{
	d = new Private(this);
}

CudaTabDlgDelegate::~CudaTabDlgDelegate()
{
	delete d;
}

Qt::WindowFlags CudaTabDlgDelegate::initWindowFlags() const
{
	return Qt::FramelessWindowHint;
}

void CudaTabDlgDelegate::create(QWidget *widget)
{
	Q_ASSERT(!d->state.contains(widget));
	d->state.insert(widget, new TabDlgState(widget));
}

void CudaTabDlgDelegate::destroy(QWidget *widget)
{
	Q_ASSERT(d->state.contains(widget));
	TabDlgState *state = d->state.value(widget);
	d->state.remove(widget);
	delete state;
}

void CudaTabDlgDelegate::tabWidgetCreated(QWidget *widget, PsiTabWidget *tabWidget)
{
	Q_UNUSED(widget);
	cuda_applyThemeAll(tabWidget);

	// FIXME: this is a strange place to put it
#ifdef Q_OS_WIN
	// cascade effect
	widget->move(get_next_pos(widget->sizeHint(), QApplication::desktop()->screenGeometry().size()));
#endif
}

bool CudaTabDlgDelegate::paintEvent(QWidget *widget, QPaintEvent *)
{
	Q_ASSERT(d->state.contains(widget));
	TabDlgState *s = d->state.value(widget);

#ifdef THEME_DEBUG
	printf("CudaFrame::paintEvent\n");
#endif

	QPainter paint(s->widget);
	s->container->draw(&paint, s->widget->width(), s->widget->height());

	paint.drawPixmap(8, 8, s->widget->windowIcon().pixmap(16, 16));

	QFont f = s->widget->font();
	f.setBold(true);
	//f.setPixelSize(10);
	QFontMetrics fm(f);
	paint.setPen(Qt::white);
	paint.setFont(f);

	paint.drawText(32, 8 + 16 - 4, s->widget->windowTitle());

	// TODO: convert to RGB?
	QImage bc = g_skin->buttonClose;
	bc.setAlphaChannel(s->makeAlpha(bc, 65));
	QImage bmax = g_skin->buttonMax;
	bmax.setAlphaChannel(s->makeAlpha(bmax, 65));
	QImage bmin = g_skin->buttonMin;
	bmin.setAlphaChannel(s->makeAlpha(bmin, 65));
	int at = s->widget->width() - 8 - bc.width();
	paint.drawImage(at, 11, bc);
	s->closeRect = QRect(at, 11, bc.width(), bc.height());
	at -= bmax.width() + 5;
	paint.drawImage(at, 11, bmax);
	s->maxRect = QRect(at, 11, bmax.width(), bmax.height());
	at -= bmin.width() + 5;
	paint.drawImage(at, 11, bmin);
	s->minRect = QRect(at, 11, bmin.width(), bmin.height());

	return true;
}

bool CudaTabDlgDelegate::resizeEvent(QWidget *widget, QResizeEvent *)
{
	Q_ASSERT(d->state.contains(widget));
	TabDlgState *s = d->state.value(widget);

	// TODO: set at start as well
	s->widget->setMask(s->container->mask(s->widget->width(), s->widget->height()));

	return false;
}

bool CudaTabDlgDelegate::mousePressEvent(QWidget *widget, QMouseEvent *event)
{
	Q_ASSERT(d->state.contains(widget));
	TabDlgState *s = d->state.value(widget);

	if(event->button() == Qt::LeftButton)
	{
		s->dragPosition = event->globalPos() - s->widget->frameGeometry().topLeft();

		QPoint at = s->widget->mapFromGlobal(event->globalPos());
#ifdef THEME_DEBUG
		printf("at: (%d,%d), size: %dx%d\n", at.x(), at.y(), width(), height());
#endif

		if(s->closeRect.contains(at))
		{
			event->accept();
			s->widget->close();
			return true;
		}
		else if(s->maxRect.contains(at))
		{
			event->accept();
			if(!s->widget->isMaximized())
				s->widget->showMaximized();
			else
				s->widget->showNormal();
			return true;
		}
		else if(s->minRect.contains(at))
		{
			event->accept();
			s->widget->showMinimized();
			return true;
		}

		if(!s->widget->isMaximized())
		{
			int zone = getResizeZone(s->widget->width(), s->widget->height(), at.x(), at.y());

			if(zone != -1)
			{
				s->dragZone = zone;
				s->resizeStartAt = event->globalPos();
				s->resizeStartRect = s->widget->geometry();
				s->resizing = true;
			}
			else
				s->moving = true;
		}

		event->accept();
	}

	if(event->isAccepted())
		return true;
	else
		return false;
}

bool CudaTabDlgDelegate::mouseMoveEvent(QWidget *widget, QMouseEvent *event)
{
	Q_ASSERT(d->state.contains(widget));
	TabDlgState *s = d->state.value(widget);

	if(event->buttons() & Qt::LeftButton)
	{
		if(s->resizing)
		{
			QPoint at = event->globalPos();
			int dragZone = s->dragZone;

			//int cx = at.x() - dragPosition.x();
			//int cy = at.y() - dragPosition.y();

			int cx = at.x() - s->resizeStartAt.x();
			int cy = at.y() - s->resizeStartAt.y();

			// ignore x-axis for top/bottom sizing
			if(dragZone == 1 || dragZone == 6)
				cx = 0;
			// ignore y-axis for left/right sizing
			else if(dragZone == 3 || dragZone == 4)
				cy = 0;

			bool movex = false;
			bool movey = false;

			if((dragZone == 0 || dragZone == 1 || dragZone == 2) && cy != 0)
				movey = true;
			if((dragZone == 0 || dragZone == 3 || dragZone == 5) && cx != 0)
				movex = true;

			int mx = 0;
			int my = 0;

			if(movex)
			{
				mx = cx;
				cx = -cx;
			}

			if(movey)
			{
				my = cy;
				cy = -cy;
			}

			int newX = s->resizeStartRect.x() + mx;
			int newY = s->resizeStartRect.y() + my;
			int newWidth = s->resizeStartRect.width() + cx;
			int newHeight = s->resizeStartRect.height() + cy;
			if(newWidth < s->widget->minimumWidth())
			{
				newX = s->resizeStartRect.x() + (s->resizeStartRect.width() - s->widget->minimumWidth());
				newWidth = s->widget->minimumWidth();
			}
			if(newHeight < s->widget->minimumHeight())
			{
				newY = s->resizeStartRect.y() + (s->resizeStartRect.height() - s->widget->minimumHeight());
				newHeight = s->widget->minimumHeight();
			}

			s->widget->setGeometry(newX, newY, newWidth, newHeight);

			//if(mx != 0 || my != 0)
			//	move(x() + mx, y() + my);

			//if(cx != 0 || cy != 0)
			//	resize(width() + cx, height() + cy);

			//dragPosition = at;
		}
		else if(s->moving)
			s->widget->move(event->globalPos() - s->dragPosition);

		event->accept();
	}

	if(!s->resizing && !s->moving && !s->widget->isMaximized())
	{
		QPoint at = s->widget->mapFromGlobal(event->globalPos());

		// cursor display
		int zone = getResizeZone(s->widget->width(), s->widget->height(), at.x(), at.y());

		if(zone == 0 || zone == 7)
			s->widget->setCursor(Qt::SizeFDiagCursor);
		else if(zone == 1 || zone == 6)
			s->widget->setCursor(Qt::SizeVerCursor);
		else if(zone == 2 || zone == 5)
			s->widget->setCursor(Qt::SizeBDiagCursor);
		else if(zone == 3 || zone == 4)
			s->widget->setCursor(Qt::SizeHorCursor);
		else
			s->widget->setCursor(Qt::ArrowCursor);

		if(zone != -1)
			s->cursorTimer.start(100);
	}

	if(event->isAccepted())
		return true;
	else
		return false;
}

bool CudaTabDlgDelegate::mouseReleaseEvent(QWidget *widget, QMouseEvent *)
{
	Q_ASSERT(d->state.contains(widget));
	TabDlgState *s = d->state.value(widget);

	s->resizing = false;
	s->moving = false;
	return false;
}

bool CudaTabDlgDelegate::event(QWidget *widget, QEvent *e)
{
	Q_ASSERT(d->state.contains(widget));
	TabDlgState *s = d->state.value(widget);

	if(e->type() == QEvent::WindowTitleChange || e->type() == QEvent::WindowIconChange)
		s->widget->update();
	return false;
}

//----------------------------------------------------------------------------
// CudaSubFrame
//----------------------------------------------------------------------------
CudaSubFrame::CudaSubFrame(QWidget *parent) : QFrame(parent)
{
	container = new CudaContainer;
	setup();
}

void CudaSubFrame::setup()
{
	container->setup(QColor(), QImage(), QImage(), g_skin->panel, QImage(), g_skin->panelNumber1, g_skin->panelNumber2, g_skin->panelNumber3, g_skin->panelNumber4);
	setContentsMargins(2, 2, 2, 2);
}

void CudaSubFrame::paintEvent(QPaintEvent *)
{
	QPainter paint(this);
	container->draw(&paint, width(), height());
}

//----------------------------------------------------------------------------
// CudaMenuBar
//----------------------------------------------------------------------------
CudaMenuBar::CudaMenuBar(QWidget *parent) : QMenuBar(parent)
{
	QColor c(0, 0, 0, 0);
	QPalette pal;
	pal.setColor(QPalette::Window, c);
	pal.setColor(QPalette::ButtonText, Qt::white);
	setPalette(pal);
}

void CudaMenuBar::paintEvent(QPaintEvent *pe)
{
	QPainter paint(this);
	paint.fillRect(0, 0, width(), height(), g_skin->menubarBg);
	QMenuBar::paintEvent(pe);
}

//----------------------------------------------------------------------------
// CudaLineEdit
//----------------------------------------------------------------------------
CudaLineEdit::CudaLineEdit(QWidget *parent) : QLineEdit(parent)
{
	setAttribute(Qt::WA_OpaquePaintEvent, true);
	//setStyle(new CudaStyle);
}

static CudaStyle *cudaStyle = 0;

QImage cuda_defaultAvatar()
{
	return g_skin->defaultAvatar;
}

static void cuda_applyTheme(QWidget *w, int depth)
{
#ifdef THEME_DEBUG
	for(int n = 0; n < depth; ++n)
		printf("  ");
	printf("%s\n", w->className());
#endif

	if(!cudaStyle)
	{
		cudaStyle = new CudaStyle;
		cudaStyle->setParent(QApplication::instance());
	}

	w->setPalette(cudaStyle->standardPalette());
	w->setStyle(cudaStyle);
}

static void cuda_applyThemeAll(QWidget *top, int depth)
{
	cuda_applyTheme(top, depth);

	QObjectList list = top->children();
	foreach(QObject *obj, list)
	{
		if(!obj->isWidgetType())
			return;

		QWidget *w = static_cast<QWidget *>(obj);
		cuda_applyThemeAll(w, depth + 1);
	}
}

void cuda_applyTheme(QWidget *w)
{
	cuda_applyTheme(w, 0);
}

void cuda_applyThemeAll(QWidget *w)
{
	cuda_applyThemeAll(w, 0);
}

QPixmap get_icon_im()
{
	return g_skin->button_icon_im;
}

QPixmap get_icon_file()
{
	return g_skin->button_icon_file;
}

QImage get_button_up()
{
	return g_skin->button_up;
}

QColor get_frameCol1()
{
	return g_skin->frameCol1;
}

QColor get_windowText()
{
	return g_skin->windowText;
}

QColor get_selectBg()
{
	return g_skin->selectBg;
}

QColor get_contactOnline()
{
	return g_skin->contactOnline;
}

QColor get_expansionLine()
{
	return g_skin->expansionLine;
}

QColor get_expansionText()
{
	return g_skin->expansionText;
}

CudaTabDlgDelegate *get_tab_delegate()
{
	return g_skin->tabdeleg;
}

#include "cudaskin.moc"
