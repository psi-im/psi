#include "mac_dock.h"

#include "ApplicationServices/ApplicationServices.h"
#include "Carbon/Carbon.h"

#define DOCK_FONT_NAME "LucidaGrande-Bold"
#define DOCK_FONT_SIZE 24
	
static NMRec bounceRec;

/**
 * Converts a QString to a CoreFoundation string, preserving Unicode.
 *
 * \param s the string to be converted.
 * \return a reference to a CoreFoundation string.
 */
/*static CFStringRef qString2CFString(const QString& s)
{
	if (s.isNull())
		return 0;
	
	ushort* buffer = new ushort[s.length()];
	for (unsigned int i = 0; i < s.length(); ++i)
		buffer[i] = s[i].unicode();
	CFStringRef result = CFStringCreateWithBytes ( NULL, 
		(UInt8*) buffer, 
		s.length()*sizeof(ushort),
		kCFStringEncodingUnicode, false);

	delete buffer;
	return result;
}*/

void MacDock::startBounce()
{
	if (!isBouncing) {
		bounceRec.qType = nmType;
		bounceRec.nmMark = 1;
		bounceRec.nmIcon = NULL;
		bounceRec.nmSound = NULL;
		bounceRec.nmStr = NULL;
		bounceRec.nmResp = NULL;
		bounceRec.nmRefCon = 0;
		NMInstall(&bounceRec);
		isBouncing = true;
	}
}

void MacDock::stopBounce()
{
	if(isBouncing) {
		NMRemove(&bounceRec);
		isBouncing = false;
	}
}

void MacDock::overlay(const QString& text)
{
	if (text.isEmpty()) {
		overlayed = false;
		RestoreApplicationDockTileImage();
		return;
	}

	// Create the context
	CGContextRef context = BeginCGContextForApplicationDockTile();

	if (!overlayed) {
		overlayed = true;

		// Add some subtle drop down shadow
		CGSize s = { 2.0, -4.0 };
		CGContextSetShadow(context, s, 5.0);
	}

	// Draw a circle
	CGContextBeginPath(context);
	CGContextAddArc(context, 95.0, 95.0, 25.0, 0.0, 2 * M_PI, true);
	CGContextClosePath(context);
	CGContextSetRGBFillColor(context, 1, 0.0, 0.0, 1);
	CGContextFillPath(context);

	// Set the clipping path to the same circle
	CGContextBeginPath(context);
	CGContextAddArc(context, 95.0, 95.0, 25.0, 0.0, 2 * M_PI, true);
	CGContextClip(context);

	// Remove drop shadow
	// FIXME: Disabled because 10.2 doesn't support it
	//CGSize s = { 0.0, -0.0 };
	//CGContextSetShadowWithColor(context, s, 0, NULL);

	// Select the appropriate font
	CGContextSelectFont(context,DOCK_FONT_NAME, DOCK_FONT_SIZE, kCGEncodingMacRoman);
	CGContextSetRGBFillColor(context, 1, 1, 1, 1);

	// Draw the text invisible
	CGPoint begin = CGContextGetTextPosition(context);
	CGContextSetTextDrawingMode(context, kCGTextInvisible);	
	CGContextShowTextAtPoint(context, begin.x, begin.y, text.toStdString().c_str(), text.length());
	CGPoint end = CGContextGetTextPosition(context);

	// Draw the text
	CGContextSetTextDrawingMode(context, kCGTextFill);	
	CGContextShowTextAtPoint(context, 95 - (end.x - begin.x)/2, 95 - 8, text.toStdString().c_str(), text.length());
	
	// Cleanup
	CGContextFlush(context);
	EndCGContextForApplicationDockTile(context);
}

bool MacDock::isBouncing = false;
bool MacDock::overlayed = false;
