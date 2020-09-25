#include <circle/chargenerator.h> 
#include <assert.h> 
#include "customfont.h" 

#define FIRSTCHAR	0x00 
#define LASTCHAR	0xFF 
#define CHARCOUNT	(LASTCHAR - FIRSTCHAR + 1)
CCharGenerator::CCharGenerator (void)
:	m_nCharWidth (width)
{
}
CCharGenerator::~CCharGenerator (void) {
}
unsigned CCharGenerator::GetCharWidth (void) const {
	return m_nCharWidth;
}
unsigned CCharGenerator::GetCharHeight (void) const {
	return height + extraheight;
}
unsigned CCharGenerator::GetUnderline (void) const {
	return height;
}
boolean CCharGenerator::GetPixel (char chAscii, unsigned nPosX, unsigned nPosY) const {
	unsigned nAscii = (u8) chAscii;
	if ( nAscii < FIRSTCHAR
	    || nAscii > LASTCHAR)
	{
		return FALSE;
	}
	unsigned nIndex = nAscii - FIRSTCHAR;
	assert (nIndex < CHARCOUNT);
	assert (nPosX < m_nCharWidth);
	if (nPosY >= height)
	{
		return FALSE;
	}
	return font_data[nIndex][nPosY] & (0x80 >> nPosX) ? TRUE : FALSE;
}
