#ifndef _ICON_H_
#define _ICON_H_

// #pragmas are used here to insure that the structure's
// packing in memory matches the packing of the EXE or DLL.
#pragma pack(push, 2)

typedef struct {
	BYTE bWidth;
	BYTE bHeight;
	BYTE bColorCount;
	BYTE bReserved;
	WORD wPlanes;
	WORD wBitCount;
	DWORD dwBytesInRes;
} ICONDIRENTRYCOMMON, *LPICONDIRENTRYCOMMON;

typedef struct {
	ICONDIRENTRYCOMMON common;
	DWORD dwImageOffset;
} ICONDIRENTRY, *LPICONDIRENTRY;

typedef struct {
	WORD idReserved;
	WORD idType;
	WORD idCount;
	ICONDIRENTRY idEntries[1];
} ICONDIR, *LPICONDIR;

typedef struct {
	ICONDIRENTRYCOMMON common;
	WORD nID;
} GRPICONDIRENTRY, *LPGRPICONDIRENTRY;

typedef struct {
	WORD idReserved;
	WORD idType;
	WORD idCount;
	GRPICONDIRENTRY idEntries[1];
} GRPICONDIR, *LPGRPICONDIR;

#pragma pack(pop)

#endif
