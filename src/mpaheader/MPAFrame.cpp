// Source: https://www.codeproject.com/script/Articles/ViewDownloads.aspx?aid=8295

/*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation; either
* version 3 of the License, or (at your option) any later version.
* 
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
* 
* You should have received a copy of the GNU Lesser General Public License
* along with this program; if not, write to the Free Software Foundation,
* Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
* 
* Copyright (c) 2007 Konrad Windszus
*/

#ifdef _WIN32
#include "util_sound_definitions.hpp"
#if USOUND_MP3_SUPPORT_ENABLED == 1
#include <Windows.h>
#include <atlstr.h>

#include <math.h>	// for ceil
#include "mpaframe.h"
#include "mpaendoffileexception.h"

#define UNUSED(a) a

// number of bits that are used for CRC check in MPEG 1 Layer 2
// (this table is invalid for Joint Stereo/Intensity Stereo)
// 1. index = allocation table index, 2. index = mono
const DWORD CMPAFrame::m_dwProtectedBitsLayer2[5][2] =
{
	{284,142},	// table a
	{308,154},	// table b
	{42,84},	// table c
	{62,114},	// table d
	{270,135}	// table for MPEG 2/2.5
};

CMPAFrame::CMPAFrame(CMPAStream* pStream, DWORD& dwOffset, bool bFindSubsequentFrame, bool bExactOffset, bool bReverse, CMPAHeader* pCompareHeader) :
	m_pStream(pStream),
	m_dwOffset(dwOffset),
	m_bIsLast(false)
{
	
	// decode header of frame at position dwOffset
	m_pHeader = new CMPAHeader(pStream, m_dwOffset, bExactOffset, bReverse, pCompareHeader);
	m_dwFrameSize = m_pHeader->CalcFrameSize();

	// do extended check?
	if (bFindSubsequentFrame)
	{	
		// calc offset for next frame header
		DWORD dwNewOffset = GetSubsequentHeaderOffset();
		try
		{
			CMPAFrame* pSubsequentFrame = new CMPAFrame(pStream, dwNewOffset, false, true, false, m_pHeader);
			delete pSubsequentFrame;
		}

		/* if no subsequent frame found
			- end of file reached (last frame)
			- corruption in file
				- subsequent frame is at incorrect position (out of detection range)
				- frame was incorrectly detected (very improbable)
		*/
		catch (CMPAEndOfFileException)
		{
			m_bIsLast = true;
		}
		catch (CMPAException& e)
		{
			UNUSED(e);
			OutputDebugString(_T("Didn't find subsequent frame"));
			//if (e->GetErrorID() == CMPAException::NoFrameInTolerance
		}
	}
	
}

CVBRHeader* CMPAFrame::FindVBRHeader() const
{
	// read out VBR header (if available), must be freed with delete
	return CVBRHeader::FindHeader(this);
}


CMPAFrame::~CMPAFrame(void)
{
	delete m_pHeader;
}


// returns true if CRC is correct otherwise false
// do only call this function, if header contains a CRC checksum
bool CMPAFrame::CheckCRC() const
{
	if (!m_pHeader->m_bCRC)
		return false;

	DWORD dwProtectedBits;
	
	// which bits should be considered for CRC calculation
	switch(m_pHeader->m_Layer)
	{
		case CMPAHeader::Layer1:
			dwProtectedBits = 4* (m_pHeader->GetNumChannels() * m_pHeader->m_wBound + (32 - m_pHeader->m_wBound));
			break;
		case CMPAHeader::Layer2:
			// no check for Layer II files (not easy to compute number protected bits, need to get allocation bits first)
			return true;
			break;
		// for Layer III the protected bits are the side information
		case CMPAHeader::Layer3:
			dwProtectedBits = m_pHeader->GetSideInfoSize()*8;
		default:
			return true;
	}

	// CRC is also calculated from the last 2 bytes of the header
	dwProtectedBits += MPA_HEADER_SIZE*8 +16;	// +16bit for CRC itself

	DWORD dwByteSize = (DWORD)ceil(dwProtectedBits/8.0); 
	
	// the first 2 bytes and the CRC itself are automatically skipped
	DWORD dwOffset = m_dwOffset;
	WORD wCRC16 = CalcCRC16(m_pStream->ReadBytes(dwByteSize, dwOffset, false), dwProtectedBits);

	// read out crc from frame (it follows frame header)
	dwOffset += + MPA_HEADER_SIZE;
	if (wCRC16 == m_pStream->ReadBEValue(2, dwOffset, false))
		return true;

	return false;
}

// CRC16 check
WORD CMPAFrame::CalcCRC16(BYTE* pBuffer, DWORD dwBitSize)
{
	DWORD n;
	WORD tmpchar, crcmask, tmpi;
	crcmask = tmpchar = 0;
	WORD crc = 0xffff;			// start with inverted value of 0

	// start with byte 2 of header
	for (n = 16;  n < dwBitSize;  n++)
	{
		if (n < 32 || n >= 48) // skip the 2 bytes of the crc itself
		{
			if (n%8 == 0)
			{
				crcmask = 1 << 8;
				tmpchar = pBuffer[n/8];
			}
			crcmask >>= 1;
			tmpi = crc & 0x8000;
			crc <<= 1;
			
			if (!tmpi ^ !(tmpchar & crcmask))
				crc ^= 0x8005;
		}
	}
	crc &= 0xffff;	// invert the result
	return crc;
}
#endif
#endif
