VERSION		EQU	2
REVISION	EQU	6

DATE	MACRO
		dc.b '21.2.2015'
		ENDM

VERS	MACRO
		dc.b '7-Zip 2.6'
		ENDM

VSTRING	MACRO
		dc.b '7-Zip 2.6 (21.2.2015)',13,10,0
		ENDM

VERSTAG	MACRO
		dc.b 0,'$VER: 7-Zip 2.6 (21.2.2015)',0
		ENDM
