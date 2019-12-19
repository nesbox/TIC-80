//
// screen.cpp
//
// Circle - A C++ bare metal environment for Raspberry Pi
// Copyright (C) 2014-2018  R. Stange <rsta2@o2online.de>
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
#include "customscreen.h"
#include <circle/devicenameservice.h>
#include <circle/synchronize.h>
#include <circle/sysconfig.h>
#include <circle/util.h>

#define ROTORS		4

enum TScreenState
{
	ScreenStateStart,
	ScreenStateEscape,
	ScreenStateBracket,
	ScreenStateNumber1,
	ScreenStateQuestionMark,
	ScreenStateSemicolon,
	ScreenStateNumber2,
	ScreenStateNumber3
};

CScreenDevice::CScreenDevice (unsigned nWidth, unsigned nHeight, boolean bVirtual)
:	m_nInitWidth (nWidth),
	m_nInitHeight (nHeight),
	m_bVirtual (bVirtual),
	m_pFrameBuffer (0),
	m_pBuffer (0),
	m_nState (ScreenStateStart),
	m_nScrollStart (0),
	m_nCursorX (0),
	m_nCursorY (0),
	m_bCursorOn (TRUE),
	m_Color (NORMAL_COLOR),
	m_bInsertOn (FALSE),
	m_bUpdated (FALSE)
#ifdef REALTIME
	, m_SpinLock (TASK_LEVEL)
#endif
{
}

CScreenDevice::~CScreenDevice (void)
{
	if (m_bVirtual)
	{
		delete [] m_pBuffer;
	}
	m_pBuffer = 0;
	
	delete m_pFrameBuffer;
	m_pFrameBuffer = 0;
}

boolean CScreenDevice::Initialize (void)
{
	if (!m_bVirtual)
	{
		m_pFrameBuffer = new CBcmFrameBuffer (m_nInitWidth, m_nInitHeight, DEPTH);
#if DEPTH == 8
		m_pFrameBuffer->SetPalette (NORMAL_COLOR, NORMAL_COLOR16);
		m_pFrameBuffer->SetPalette (HIGH_COLOR,   HIGH_COLOR16);
		m_pFrameBuffer->SetPalette (HALF_COLOR,   HALF_COLOR16);
#endif
		if (!m_pFrameBuffer->Initialize ())
		{
			return FALSE;
		}

		if (m_pFrameBuffer->GetDepth () != DEPTH)
		{
			return FALSE;
		}

		m_pBuffer = (TScreenColor *) (uintptr) m_pFrameBuffer->GetBuffer ();
		m_nSize   = m_pFrameBuffer->GetSize ();
		m_nPitch  = m_pFrameBuffer->GetPitch ();
		m_nWidth  = m_pFrameBuffer->GetWidth ();
		m_nHeight = m_pFrameBuffer->GetHeight ();

		// Ensure that each row is word-aligned so that we can safely use memcpyblk()
		if (m_nPitch % sizeof (u32) != 0)
		{
			return FALSE;
		}
		m_nPitch /= sizeof (TScreenColor);
	}
	else
	{
		m_nWidth = m_nInitWidth;
		m_nHeight = m_nInitHeight;
		m_nSize = m_nWidth * m_nHeight * sizeof (TScreenColor);
		m_nPitch = m_nWidth;

		m_pBuffer = new TScreenColor[m_nWidth * m_nHeight];
	}

	m_nUsedHeight = m_nHeight / m_CharGen.GetCharHeight () * m_CharGen.GetCharHeight ();
	m_nScrollEnd = m_nUsedHeight;

	CursorHome ();
	ClearDisplayEnd ();
	InvertCursor ();

	CDeviceNameService::Get ()->AddDevice ("tty1", this, FALSE);

	return TRUE;
}

unsigned CScreenDevice::GetWidth (void) const
{
	return m_nWidth;
}
unsigned CScreenDevice::GetPitch (void) const
{
	return m_nPitch;
}

unsigned CScreenDevice::GetHeight (void) const
{
	return m_nHeight;
}

unsigned CScreenDevice::GetColumns (void) const
{
	return m_nWidth / m_CharGen.GetCharWidth ();
}

unsigned CScreenDevice::GetRows (void) const
{
	return m_nUsedHeight / m_CharGen.GetCharHeight ();
}

TScreenStatus CScreenDevice::GetStatus (void)
{
	TScreenStatus Status;

	Status.pContent   = m_pBuffer;
	Status.nSize	  = m_nSize;
	Status.nState     = m_nState;
	Status.nScrollStart = m_nScrollStart;
	Status.nScrollEnd   = m_nScrollEnd;
	Status.nCursorX   = m_nCursorX;
	Status.nCursorY   = m_nCursorY;
	Status.bCursorOn  = m_bCursorOn;
	Status.Color      = m_Color;
	Status.bInsertOn  = m_bInsertOn;
	Status.nParam1    = m_nParam1;
	Status.nParam2    = m_nParam2;
	Status.bUpdated   = m_bUpdated;

	return Status;
}

boolean CScreenDevice::SetStatus (const TScreenStatus &Status)
{
	if (   m_nSize  != Status.nSize
	    || m_nPitch != m_nWidth)
	{
		return FALSE;
	}

	m_SpinLock.Acquire ();

	if (   m_bUpdated
	    || Status.bUpdated)
	{
		m_SpinLock.Release ();

		return FALSE;
	}

	memcpyblk (m_pBuffer, Status.pContent, m_nSize);

	m_nState     = Status.nState;
	m_nScrollStart = Status.nScrollStart;
	m_nScrollEnd   = Status.nScrollEnd;
	m_nCursorX   = Status.nCursorX;
	m_nCursorY   = Status.nCursorY;
	m_bCursorOn  = Status.bCursorOn;
	m_Color      = Status.Color;
	m_bInsertOn  = Status.bInsertOn;
	m_nParam1    = Status.nParam1;
	m_nParam2    = Status.nParam2;

	m_SpinLock.Release ();

	DataMemBarrier ();

	return TRUE;
}

int CScreenDevice::Write (const void *pBuffer, size_t nCount)
{
	m_SpinLock.Acquire ();

	m_bUpdated = TRUE;
	
	InvertCursor ();
	
	const char *pChar = (const char *) pBuffer;
	int nResult = 0;

	while (nCount--)
	{
		Write (*pChar++);
		
		nResult++;
	}

	InvertCursor ();
	
	m_bUpdated = FALSE;

	m_SpinLock.Release ();

	DataMemBarrier ();

	return nResult;
}

void CScreenDevice::Write (char chChar)
{
	switch (m_nState)
	{
	case ScreenStateStart:
		switch (chChar)
		{
		case '\b':
			CursorLeft ();
			break;

		case '\t':
			Tabulator ();
			break;

		case '\n':
			NewLine ();
			break;

		case '\r':
			CarriageReturn ();
			break;

		case '\x1b':
			m_nState = ScreenStateEscape;
			break;

		default:
			DisplayChar (chChar);
			break;
		}
		break;

	case ScreenStateEscape:
		switch (chChar)
		{
		case 'M':
			ReverseScroll ();
			m_nState = ScreenStateStart;
			break;

		case '[':
			m_nState = ScreenStateBracket;
			break;

		default:
			m_nState = ScreenStateStart;
			break;
		}
		break;

	case ScreenStateBracket:
		switch (chChar)
		{
		case '?':
			m_nState = ScreenStateQuestionMark;
			break;

		case 'A':
			CursorUp ();
			m_nState = ScreenStateStart;
			break;

		case 'B':
			CursorDown ();
			m_nState = ScreenStateStart;
			break;

		case 'C':
			CursorRight ();
			m_nState = ScreenStateStart;
			break;

		case 'D':
			CursorLeft ();
			m_nState = ScreenStateStart;
			break;

		case 'H':
			CursorHome ();
			m_nState = ScreenStateStart;
			break;

		case 'J':
			ClearDisplayEnd ();
			m_nState = ScreenStateStart;
			break;

		case 'K':
			ClearLineEnd ();
			m_nState = ScreenStateStart;
			break;

		case 'L':
			InsertLines (1);
			m_nState = ScreenStateStart;
			break;

		case 'M':
			DeleteLines (1);
			m_nState = ScreenStateStart;
			break;

		case 'P':
			DeleteChars (1);
			m_nState = ScreenStateStart;
			break;

		default:
			if ('0' <= chChar && chChar <= '9')
			{
				m_nParam1 = chChar - '0';
				m_nState = ScreenStateNumber1;
			}
			else
			{
				m_nState = ScreenStateStart;
			}
			break;
		}
		break;

	case ScreenStateNumber1:
		switch (chChar)
		{
		case ';':
			m_nState = ScreenStateSemicolon;
			break;

		case 'L':
			InsertLines (m_nParam1);
			m_nState = ScreenStateStart;
			break;

		case 'M':
			DeleteLines (m_nParam1);
			m_nState = ScreenStateStart;
			break;

		case 'P':
			DeleteChars (m_nParam1);
			m_nState = ScreenStateStart;
			break;

		case 'X':
			EraseChars (m_nParam1);
			m_nState = ScreenStateStart;
			break;

		case 'h':
		case 'l':
			if (m_nParam1 == 4)
			{
				InsertMode (chChar == 'h');
			}
			m_nState = ScreenStateStart;
			break;
			
		case 'm':
			SetStandoutMode (m_nParam1);
			m_nState = ScreenStateStart;
			break;

		default:
			if ('0' <= chChar && chChar <= '9')
			{
				m_nParam1 *= 10;
				m_nParam1 += chChar - '0';

				if (m_nParam1 > 99)
				{
					m_nState = ScreenStateStart;
				}
			}
			else
			{
				m_nState = ScreenStateStart;
			}
			break;
		}
		break;

	case ScreenStateSemicolon:
		if ('0' <= chChar && chChar <= '9')
		{
			m_nParam2 = chChar - '0';
			m_nState = ScreenStateNumber2;
		}
		else
		{
			m_nState = ScreenStateStart;
		}
		break;

	case ScreenStateQuestionMark:
		if ('0' <= chChar && chChar <= '9')
		{
			m_nParam1 = chChar - '0';
			m_nState = ScreenStateNumber3;
		}
		else
		{
			m_nState = ScreenStateStart;
		}
		break;

	case ScreenStateNumber2:
		switch (chChar)
		{
		case 'H':
			CursorMove (m_nParam1, m_nParam2);
			m_nState = ScreenStateStart;
			break;

		case 'r':
			SetScrollRegion (m_nParam1, m_nParam2);
			m_nState = ScreenStateStart;
			break;

		default:
			if ('0' <= chChar && chChar <= '9')
			{
				m_nParam2 *= 10;
				m_nParam2 += chChar - '0';

				if (m_nParam2 > 199)
				{
					m_nState = ScreenStateStart;
				}
			}
			else
			{
				m_nState = ScreenStateStart;
			}
			break;
		}
		break;

	case ScreenStateNumber3:
		switch (chChar)
		{
		case 'h':
		case 'l':
			if (m_nParam1 == 25)
			{
				SetCursorMode (chChar == 'h');
			}
			m_nState = ScreenStateStart;
			break;

		default:
			if ('0' <= chChar && chChar <= '9')
			{
				m_nParam1 *= 10;
				m_nParam1 += chChar - '0';

				if (m_nParam1 > 99)
				{
					m_nState = ScreenStateStart;
				}
			}
			else
			{
				m_nState = ScreenStateStart;
			}
			break;
		}
		break;

	default:
		m_nState = ScreenStateStart;
		break;
	}
}

void CScreenDevice::CarriageReturn (void)
{
	m_nCursorX = 0;
}

void CScreenDevice::ClearDisplayEnd (void)
{
	ClearLineEnd ();
	
	unsigned nPosY = m_nCursorY + m_CharGen.GetCharHeight ();
	unsigned nOffset = nPosY * m_nPitch;
	
	TScreenColor *pBuffer = m_pBuffer + nOffset;
	unsigned nSize = m_nSize / sizeof (TScreenColor) - nOffset;
	
	while (nSize--)
	{
		*pBuffer++ = BLACK_COLOR;
	}
}

void CScreenDevice::ClearLineEnd (void)
{
	for (unsigned nPosX = m_nCursorX; nPosX < m_nWidth; nPosX += m_CharGen.GetCharWidth ())
	{
		EraseChar (nPosX, m_nCursorY);
	}
}

void CScreenDevice::CursorDown (void)
{
	m_nCursorY += m_CharGen.GetCharHeight ();
	if (m_nCursorY >= m_nScrollEnd)
	{
		Scroll ();

		m_nCursorY -= m_CharGen.GetCharHeight ();
	}
}

void CScreenDevice::CursorHome (void)
{
	m_nCursorX = 0;
	m_nCursorY = m_nScrollStart;
}

void CScreenDevice::CursorLeft (void)
{
	if (m_nCursorX > 0)
	{
		m_nCursorX -= m_CharGen.GetCharWidth ();
	}
	else
	{
		if (m_nCursorY > m_nScrollStart)
		{
			m_nCursorX = m_nWidth - m_CharGen.GetCharWidth ();
			m_nCursorY -= m_CharGen.GetCharHeight ();
		}
	}
}

void CScreenDevice::CursorMove (unsigned nRow, unsigned nColumn)
{
	unsigned nPosX = (nColumn - 1) * m_CharGen.GetCharWidth ();
	unsigned nPosY = (nRow - 1) * m_CharGen.GetCharHeight ();

	if (   nPosX < m_nWidth
	    && nPosY >= m_nScrollStart
	    && nPosY < m_nScrollEnd)
	{
		m_nCursorX = nPosX;
		m_nCursorY = nPosY;
	}
}

void CScreenDevice::CursorRight (void)
{
	m_nCursorX += m_CharGen.GetCharWidth ();
	if (m_nCursorX >= m_nWidth)
	{
		NewLine ();
	}
}

void CScreenDevice::CursorUp (void)
{
	if (m_nCursorY > m_nScrollStart)
	{
		m_nCursorY -= m_CharGen.GetCharHeight ();
	}
}

void CScreenDevice::DeleteChars (unsigned nCount)	// TODO
{
}

void CScreenDevice::DeleteLines (unsigned nCount)	// TODO
{
}

void CScreenDevice::DisplayChar (char chChar)
{
	// TODO: Insert mode
	
	if (' ' <= (unsigned char) chChar)
	{
		DisplayChar (chChar, m_nCursorX, m_nCursorY, m_Color);

		CursorRight ();
	}
}

void CScreenDevice::EraseChars (unsigned nCount)
{
	if (nCount == 0)
	{
		return;
	}

	unsigned nEndX = m_nCursorX + nCount * m_CharGen.GetCharWidth ();
	if (nEndX > m_nWidth)
	{
		nEndX = m_nWidth;
	}

	for (unsigned nPosX = m_nCursorX; nPosX < nEndX; nPosX += m_CharGen.GetCharWidth ())
	{
		EraseChar (nPosX, m_nCursorY);
	}
}

void CScreenDevice::InsertLines (unsigned nCount)	// TODO
{
}

void CScreenDevice::InsertMode (boolean bBegin)
{
	m_bInsertOn = bBegin;
}

void CScreenDevice::NewLine (void)
{
	CarriageReturn ();
	CursorDown ();
}

void CScreenDevice::ReverseScroll (void)
{
	if (   m_nCursorX == 0
	    && m_nCursorY == 0
	)
	{
		InsertLines (1);
	}
}

void CScreenDevice::SetCursorMode (boolean bVisible)
{
	m_bCursorOn = bVisible;
}

void CScreenDevice::SetScrollRegion (unsigned nStartRow, unsigned nEndRow)
{
	unsigned nScrollStart = (nStartRow - 1) * m_CharGen.GetCharHeight ();
	unsigned nScrollEnd   = nEndRow * m_CharGen.GetCharHeight ();

	if (   nScrollStart <  m_nUsedHeight
	    && nScrollEnd   >  0
	    && nScrollEnd   <= m_nUsedHeight
	    && nScrollStart <  nScrollEnd)
	{
		m_nScrollStart = nScrollStart;
		m_nScrollEnd   = nScrollEnd;
	}

	CursorHome ();
}

// TODO: standout mode should be useable together with one other mode
void CScreenDevice::SetStandoutMode (unsigned nMode)
{
	switch (nMode)
	{
	case 0:
	case 27:
		m_Color = NORMAL_COLOR;
		break;
		
	case 1:
		m_Color = HIGH_COLOR;
		break;

	case 2:
		m_Color = HALF_COLOR;
		break;

	case 7:				// TODO: reverse mode
	default:
		break;
	}
}

void CScreenDevice::Tabulator (void)
{
	unsigned nTabWidth = m_CharGen.GetCharWidth () * 8;
	
	m_nCursorX = ((m_nCursorX + nTabWidth) / nTabWidth) * nTabWidth;
	if (m_nCursorX >= m_nWidth)
	{
		NewLine ();
	}
}

void CScreenDevice::Scroll (void)
{
	unsigned nLines = m_CharGen.GetCharHeight ();

	u32 *pTo = (u32 *) (m_pBuffer + m_nScrollStart * m_nPitch);
	u32 *pFrom = (u32 *) (m_pBuffer + (m_nScrollStart + nLines) * m_nPitch);

	unsigned nSize = m_nPitch * (m_nScrollEnd - m_nScrollStart - nLines) * sizeof (TScreenColor);
	if (nSize > 0)
	{
		unsigned nSizeBlk = nSize & ~0xF;
		memcpyblk (pTo, pFrom, nSizeBlk);

		// Handle framebuffers with row lengths not aligned to 16 bytes
		memcpy ((u8 *) pTo + nSizeBlk, (u8 *) pFrom + nSizeBlk, nSize & 0xF);

		pTo += nSize / sizeof (u32);
	}

	nSize = m_nPitch * nLines * sizeof (TScreenColor) / sizeof (u32);
	while (nSize--)
	{
		*pTo++ = BLACK_COLOR;
	}
}

void CScreenDevice::DisplayChar (char chChar, unsigned nPosX, unsigned nPosY, TScreenColor Color)
{
	for (unsigned y = 0; y < m_CharGen.GetCharHeight (); y++)
	{
		for (unsigned x = 0; x < m_CharGen.GetCharWidth (); x++)
		{
			SetPixel (nPosX + x, nPosY + y,
				  m_CharGen.GetPixel (chChar, x, y) ? Color : BLACK_COLOR);
		}
	}
}

void CScreenDevice::EraseChar (unsigned nPosX, unsigned nPosY)
{
	for (unsigned y = 0; y < m_CharGen.GetCharHeight (); y++)
	{
		for (unsigned x = 0; x < m_CharGen.GetCharWidth (); x++)
		{
			SetPixel (nPosX + x, nPosY + y, BLACK_COLOR);
		}
	}
}

void CScreenDevice::InvertCursor (void)
{
	if (!m_bCursorOn)
	{
		return;
	}
	
	for (unsigned y = m_CharGen.GetUnderline (); y < m_CharGen.GetCharHeight (); y++)
	{
		for (unsigned x = 0; x < m_CharGen.GetCharWidth (); x++)
		{
			if (GetPixel (m_nCursorX + x, m_nCursorY + y) == BLACK_COLOR)
			{
				SetPixel (m_nCursorX + x, m_nCursorY + y, m_Color);
			}
			else
			{
				SetPixel (m_nCursorX + x, m_nCursorY + y, BLACK_COLOR);
			}
		}
	}
}

void CScreenDevice::SetPixel (unsigned nPosX, unsigned nPosY, TScreenColor Color)
{
	if (   nPosX < m_nWidth
	    && nPosY < m_nHeight)
	{
		m_pBuffer[m_nPitch * nPosY + nPosX] = Color;
	}
}

TScreenColor CScreenDevice::GetPixel (unsigned nPosX, unsigned nPosY)
{
	if (   nPosX < m_nWidth
	    && nPosY < m_nHeight)
	{
		return m_pBuffer[m_nPitch * nPosY + nPosX];
	}
	
	return BLACK_COLOR;
}

void CScreenDevice::Rotor (unsigned nIndex, unsigned nCount)
{
	static const char chChars[] = "-\\|/";

	nIndex %= ROTORS;
	nCount &= 4-1;

	unsigned nPosX = m_nWidth - (nIndex + 1) * m_CharGen.GetCharWidth ();

	DisplayChar (chChars[nCount], nPosX, 0, HIGH_COLOR);
}

void CScreenDevice::vsync ()
{
 m_pFrameBuffer->WaitForVerticalSync();
}


TScreenColor* CScreenDevice::GetBuffer ()
{
      return m_pBuffer;
}

