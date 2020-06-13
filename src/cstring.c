#include<plibc/cstring.h>
#include<plibc/string.h>
#include<mem/kernel_alloc.h>


#define FORMAT_RESERVE		64	// additional bytes to allocate

#define MAX_NUMBER_LEN		22	// 64 bit octal number
#define MAX_PRECISION		9	// floor (log10 (ULONG_MAX))
#define MAX_FLOAT_LEN		(1+MAX_NUMBER_LEN+1+MAX_PRECISION)




void ReserveSpace (c_string_t *cstr, size_t nSpace)
{
	if (nSpace == 0)
	{
		return;
	}
	
	size_t nOffset = cstr->m_pInPtr - cstr->buffer;
	size_t nNewSize = nOffset + nSpace + 1;
	if (cstr->buffer_size >= nNewSize)
	{
		return;
	}
	
	nNewSize += FORMAT_RESERVE;
	char *pNewBuffer = kernel_allocate(nNewSize);
		
	*(cstr->m_pInPtr) = '\0';
	strcpy (pNewBuffer, cstr->buffer);
	
	kernel_deallocate(cstr->buffer);

	cstr->buffer = pNewBuffer;
	cstr->buffer_size = nNewSize;

	cstr->m_pInPtr = cstr->buffer + nOffset;
}


static void PutChar (c_string_t *cstr, char chChar, size_t nCount)
{
	ReserveSpace (cstr, nCount);

	while (nCount--)
	{
		*cstr->m_pInPtr++ = chChar;
	}
}

static void PutString (c_string_t *cstr, const char *pString)
{
	size_t nLen = strlen (pString);
	
	ReserveSpace (cstr, nLen);
	
	strcpy (cstr->m_pInPtr, pString);
	
	cstr->m_pInPtr += nLen;
}

c_string_t* get_new_cstring (const void *char_buf) {
    c_string_t * cstr = kernel_allocate(sizeof(c_string_t));
    if(cstr == 0) {
        return 0;
    }
	cstr->buffer_size = strlen (char_buf)+1;

	cstr->buffer = kernel_allocate(cstr->buffer_size);
    if(cstr->buffer == 0) {
        return 0;
    }
	strcpy (cstr->buffer, char_buf);
    return cstr;
}

void deallocate_cstring(c_string_t *cstr) {
    kernel_deallocate(cstr->buffer);
    kernel_deallocate(cstr);
}


bool reinit_cstring (c_string_t * cstr, const void *char_buf) 
{
	kernel_deallocate(cstr->buffer);

	cstr->buffer_size = strlen (char_buf) + 1;

	cstr->buffer = kernel_allocate(cstr->buffer_size);
    if(cstr->buffer == 0) {
        return false;
    }
	strcpy (cstr->buffer, char_buf);

	return true;
}

c_string_t * deep_copy_cstring (c_string_t * cstr) {
    return get_new_cstring(cstr->buffer);
}

unsigned int get_length (c_string_t * cstr) {
    if(cstr->buffer_size == 0) {
        return 0;
    }
    return strlen(cstr->buffer);
}

void append_to_cstring(c_string_t *cstr, const void *char_buf)
{
	uint32_t m_nSize = 1;
	if (cstr->buffer != 0)
	{
		m_nSize += strlen (cstr->buffer);
	}
	m_nSize += strlen (char_buf);

	char *pBuffer = kernel_allocate(m_nSize);

	if (cstr->buffer != 0)
	{
		strcpy (pBuffer, cstr->buffer);
		kernel_deallocate(cstr->buffer);
	}
	else
	{
		*pBuffer = '\0';
	}

	strcat (pBuffer, char_buf);

	cstr->buffer = pBuffer;
    cstr->buffer_size = m_nSize;
}

int compare_cstrings(c_string_t *cstr1, c_string_t* cstr2) {
	return strcmp (cstr1->buffer, cstr2->buffer);
}

int find_in_cstrings(c_string_t *cstr, char c) {
	int nPos = 0;

	for (char *p = cstr->buffer; *p; p++)
	{
		if (*p == c)
		{
			return nPos;
		}

		nPos++;
	}

	return -1;
}

int replace_in_cstring (c_string_t *cstr, const char *pOld, const char *pNew) {
	int nResult = 0;

	if (*pOld == '\0')
	{
		return nResult;
	}

	c_string_t *OldString = get_new_cstring(cstr->buffer);

	kernel_deallocate(cstr->buffer);
    
	cstr->buffer_size = FORMAT_RESERVE;
	cstr->buffer = kernel_allocate(cstr->buffer_size);
	cstr->m_pInPtr = cstr->buffer;

	const char *pReader = OldString->buffer;
	const char *pFound;
	while ((pFound = strchr (pReader, pOld[0])) != 0)
	{
		while (pReader < pFound)
		{
			PutChar (cstr, *pReader++, 1);
		}

		const char *pPattern = pOld+1;
		const char *pCompare = pFound+1;
		while (*pPattern != '\0' && *pCompare == *pPattern)
		{
			pCompare++;
			pPattern++;
		}

		if (*pPattern == '\0')
		{
			PutString (cstr, pNew);
			pReader = pCompare;
			nResult++;
		}
		else
		{
			PutChar (cstr, *pReader++, 1);
		}
	}

	PutString (cstr, pReader);

	*(cstr->m_pInPtr) = '\0';

	return nResult;
}

void cstring_format (c_string_t *cstr, const char *pFormat, ...) {
	va_list var;
	va_start (var, pFormat);

	cstring_formatv(cstr, pFormat, var);

	va_end (var);
}

void cstring_formatv(c_string_t *cstr, const char *pFormat, va_list Args)
{

    kernel_deallocate(cstr->buffer);

	cstr->buffer_size = FORMAT_RESERVE;
	cstr->buffer = kernel_allocate(cstr->buffer_size);
	cstr->m_pInPtr = cstr->buffer;

	while (*pFormat != '\0')
	{
		if (*pFormat == '%')
		{
			if (*++pFormat == '%')
			{
				PutChar (cstr, '%', 1);
				
				pFormat++;

				continue;
			}

			bool bAlternate = false;
			if (*pFormat == '#')
			{
				bAlternate = true;

				pFormat++;
			}

			bool bLeft = false;
			if (*pFormat == '-')
			{
				bLeft = true;

				pFormat++;
			}

			bool bNull = false;
			if (*pFormat == '0')
			{
				bNull = true;

				pFormat++;
			}

			size_t nWidth = 0;
			while ('0' <= *pFormat && *pFormat <= '9')
			{
				nWidth = nWidth * 10 + (*pFormat - '0');

				pFormat++;
			}

			unsigned nPrecision = 6;
			if (*pFormat == '.')
			{
				pFormat++;

				nPrecision = 0;
				while ('0' <= *pFormat && *pFormat <= '9')
				{
					nPrecision = nPrecision * 10 + (*pFormat - '0');

					pFormat++;
				}
			}
            bool bLong = false;
			bool bLongLong = false;
			unsigned long long ullArg;
			long long llArg = 0;

			if (*pFormat == 'l')
			{
				if (*(pFormat+1) == 'l')
				{
					bLongLong = true;

					pFormat++;
				}
				else
				{
					bLong = true;
				}

				pFormat++;
			}
			char chArg;
			const char *pArg;
			unsigned long ulArg;
			size_t nLen;
			unsigned nBase;
			char NumBuf[MAX_FLOAT_LEN+1];
			bool bMinus = false;
			long lArg = 0;
			double fArg;

			switch (*pFormat)
			{
			case 'c':
				chArg = (char) va_arg (Args, int);
				if (bLeft)
				{
					PutChar (cstr, chArg, 1);
					if (nWidth > 1)
					{
						PutChar (cstr, ' ', nWidth-1);
					}
				}
				else
				{
					if (nWidth > 1)
					{
						PutChar (cstr, ' ', nWidth-1);
					}
					PutChar (cstr, chArg, 1);
				}
				break;

			case 'd':
			case 'i':
				if (bLongLong)
				{
					llArg = va_arg (Args, long long);
				}
				else if (bLong)
				{
					lArg = va_arg (Args, long);
				}
				else
				{
					lArg = va_arg (Args, int);
				}
				if (lArg < 0)
				{
					bMinus = true;
					lArg = -lArg;
				}
				if (llArg < 0)
				{
					bMinus = true;
					llArg = -llArg;
				}
				if (bLongLong) {
                    cstring_lltoa (NumBuf, (unsigned long long) llArg, 10, false);
                } else {
                    cstring_ntoa (NumBuf, (unsigned long) lArg, 10, false);
                }
					

				nLen = strlen (NumBuf) + (bMinus ? 1 : 0);
				if (bLeft)
				{
					if (bMinus)
					{
						PutChar (cstr, '-', 1);
					}
					PutString (cstr, NumBuf);
					if (nWidth > nLen)
					{
						PutChar (cstr, ' ', nWidth-nLen);
					}
				}
				else
				{
					if (!bNull)
					{
						if (nWidth > nLen)
						{
							PutChar (cstr, ' ', nWidth-nLen);
						}
						if (bMinus)
						{
							PutChar (cstr, '-', 1);
						}
					}
					else
					{
						if (bMinus)
						{
							PutChar (cstr, '-', 1);
						}
						if (nWidth > nLen)
						{
							PutChar (cstr, '0', nWidth-nLen);
						}
					}
					PutString (cstr, NumBuf);
				}
				break;

			case 'f':
				fArg = va_arg (Args, double);
				cstring_ftoa (NumBuf, fArg, nPrecision);
				nLen = strlen (NumBuf);
				if (bLeft)
				{
					PutString (cstr, NumBuf);
					if (nWidth > nLen)
					{
						PutChar (cstr, ' ', nWidth-nLen);
					}
				}
				else
				{
					if (nWidth > nLen)
					{
						PutChar (cstr,' ', nWidth-nLen);
					}
					PutString (cstr, NumBuf);
				}
				break;

			case 'o':
				if (bAlternate)
				{
					PutChar (cstr,'0', 1);
				}
				nBase = 8;
				goto FormatNumber;

			case 's':
				pArg = va_arg (Args, const char *);
				nLen = strlen (pArg);
				if (bLeft)
				{
					PutString(cstr, pArg);
					if (nWidth > nLen)
					{
						PutChar (cstr, ' ', nWidth-nLen);
					}
				}
				else
				{
					if (nWidth > nLen)
					{
						PutChar (cstr,' ', nWidth-nLen);
					}
					PutString (cstr, pArg);
				}
				break;

			case 'u':
				nBase = 10;
				goto FormatNumber;

			case 'x':
			case 'X':
			case 'p':
				if (bAlternate)
				{
					PutString (cstr, *pFormat == 'X' ? "0X" : "0x");
				}
				nBase = 16;
				goto FormatNumber;

			FormatNumber:
				if (bLongLong)
				{
					ullArg = va_arg (Args, unsigned long long);
				}
				else if (bLong)
				{
					ulArg = va_arg (Args, unsigned long);
				}
				else
				{
					ulArg = va_arg (Args, unsigned);
				}

				if (bLongLong) {
                    cstring_lltoa (NumBuf, ullArg, nBase, *pFormat == 'X');
                } else {
                    cstring_ntoa (NumBuf, ulArg, nBase, *pFormat == 'X');
                }

				nLen = strlen (NumBuf);
				if (bLeft)
				{
					PutString (cstr, NumBuf);
					if (nWidth > nLen)
					{
						PutChar (cstr, ' ', nWidth-nLen);
					}
				}
				else
				{
					if (nWidth > nLen)
					{
						PutChar (cstr, bNull ? '0' : ' ', nWidth-nLen);
					}
					PutString (cstr, NumBuf);
				}
				break;

			default:
				PutChar (cstr, '%', 1);
				PutChar (cstr, *pFormat, 1);
				break;
			}
		}
		else
		{
			PutChar (cstr, *pFormat, 1);
		}

		pFormat++;
	}

	*cstr->m_pInPtr = '\0';
}


char *  cstring_ntoa (char *pDest, unsigned long ulNumber, unsigned nBase, bool bUpcase)
{
	unsigned long ulDigit;

	unsigned long ulDivisor = 1UL;
	while (1)
	{
		ulDigit = ulNumber / ulDivisor;
		if (ulDigit < nBase)
		{
			break;
		}

		ulDivisor *= nBase;
	}

	char *p = pDest;
	while (1)
	{
		ulNumber %= ulDivisor;

		*p++ = ulDigit < 10 ? '0' + ulDigit : '0' + ulDigit + 7 + (bUpcase ? 0 : 0x20);

		ulDivisor /= nBase;
		if (ulDivisor == 0)
		{
			break;
		}

		ulDigit = ulNumber / ulDivisor;
	}

	*p = '\0';

	return pDest;
}


char * cstring_lltoa (char *pDest, unsigned long long ullNumber, unsigned nBase, bool bUpcase)
{
	unsigned long long ullDigit;

	unsigned long long ullDivisor = 1ULL;
	while (1)
	{
		ullDigit = ullNumber / ullDivisor;
		if (ullDigit < nBase)
		{
			break;
		}

		ullDivisor *= nBase;
	}

	char *p = pDest;
	while (1)
	{
		ullNumber %= ullDivisor;

		*p++ = ullDigit < 10 ? '0' + ullDigit : '0' + ullDigit + 7 + (bUpcase ? 0 : 0x20);

		ullDivisor /= nBase;
		if (ullDivisor == 0)
		{
			break;
		}

		ullDigit = ullNumber / ullDivisor;
	}

	*p = '\0';

	return pDest;
}

char * cstring_ftoa (char *pDest, double fNumber, unsigned nPrecision)
{
	char *p = pDest;

	if (fNumber < 0)
	{
		*p++ = '-';

		fNumber = -fNumber;
	}

	if (fNumber > (unsigned long) -1)
	{
		strcpy (p, "overflow");

		return pDest;
	}

	unsigned long iPart = (unsigned long) fNumber;
	cstring_ntoa (p, iPart, 10, false);

	if (nPrecision == 0)
	{
		return pDest;
	}

	p += strlen (p);
	*p++ = '.';

	if (nPrecision > MAX_PRECISION)
	{
		nPrecision = MAX_PRECISION;
	}

	unsigned long nPrecPow10 = 10;
	for (unsigned i = 2; i <= nPrecision; i++)
	{
		nPrecPow10 *= 10;
	}

	fNumber -= (double) iPart;
	fNumber *= (double) nPrecPow10;
	
	char Buffer[MAX_PRECISION+1];
	cstring_ntoa (Buffer, (unsigned long) fNumber, 10, false);

	nPrecision -= strlen (Buffer);
	while (nPrecision--)
	{
		*p++ = '0';
	}

	strcpy (p, Buffer);
	
	return pDest;
}
