#ifndef __MCP2515_PRIVATE_H__
#define __MCP2515_PRIVATE_H__

// Информация по доступу к регистрам MCP2515

//
// Регистры
//
#define BFPCTRL     0x0C
#define TXRTSCTRL   0x0D
#define CANSTAT     0x0E
#define CANCTRL     0x0F
#define TEC         0x1C
#define REC         0x1D
#define CNF3        0x28
#define CNF2        0x29
#define CNF1        0x2A
#define CANINTE     0x2B
#define CANINTF     0x2C
#define EFLG        0x2D


#define RXFnSIDH(n) (((n) <= 2) ? (0x00 + (n) * 4) : (0x10 + ((n) - 3) * 4))
#define RXFnSIDL(n) (((n) <= 2) ? (0x01 + (n) * 4) : (0x11 + ((n) - 3) * 4))
#define RXFnEID8(n) (((n) <= 2) ? (0x02 + (n) * 4) : (0x12 + ((n) - 3) * 4))
#define RXFnEID0(n) (((n) <= 2) ? (0x03 + (n) * 4) : (0x13 + ((n) - 3) * 4))
#define NB_FILTERS  6

#define RXMnSIDH(n) (0x20 + ((n) * 4))
#define RXMnSIDL(n) (0x21 + ((n) * 4))
#define RXMnEID8(n) (0x22 + ((n) * 4))
#define RXMnEID0(n) (0x23 + ((n) * 4))
#define NB_MASKS    2

#define TXBnCTRL(n) (0x30 + ((n) << 4))
#define TXBnSIDH(n) (0x31 + ((n) << 4))
#define TXBnSIDL(n) (0x32 + ((n) << 4))
#define TXBnEID8(n) (0x33 + ((n) << 4))
#define TXBnEID0(n) (0x34 + ((n) << 4))
#define TXBnDLC(n)  (0x35 + ((n) << 4))
#define TXBnDm(n,m) (0x36 + ((n) << 4) + (m))
#define NB_TX_BUFFERS   3

#define RXBnCTRL(n) (0x60 + ((n) << 4))
#define RXBnSIDH(n) (0x61 + ((n) << 4))
#define RXBnSIDL(n) (0x62 + ((n) << 4))
#define RXBnEID8(n) (0x63 + ((n) << 4))
#define RXBnEID0(n) (0x64 + ((n) << 4))
#define RXBnDLC(n)  (0x65 + ((n) << 4))
#define RXBnDm(n,m) (0x66 + ((n) << 4) + (m))
#define NB_RX_BUFFERS   2

//
// Биты
//

// BFPCTRL
#define BnBFS(n)    ((1 << (n)) << 4)
#define BnBFE(n)    ((1 << (n)) << 2)
#define BnBFM(n)    ((1 << (n)) << 0)

// TXRTSCTRL
#define BnRTS(n)    ((1 << (n)) << 3)
#define BnRTSM(n)   ((1 << (n)) << 0)

// SIDL
#define SRR         (1 << 4)
#define EXIDE       (1 << 3)

// DLC
#define RTR         (1 << 6)
#define LEN_MASK    0xF

// CANSTAT
#define OPMOD(r)            (((r) >> 5) & 7)
#define OPMOD_NORMAL        0
#define OPMOD_SLEEP         1
#define OPMOD_LOOPBACK      2
#define OPMOD_LISTEN_ONLY   3
#define OPMOD_CONFIG        4
#define OPMOD_MASK          (7 << 5)

#define GET_ICOD(r)         (((r) >> 1) & 7)
#define ICOD_NONE           0
#define ICOD_ERROR          1
#define ICOD_WAKE_UP        2
#define ICOD_TXB0           3
#define ICOD_TXB1           4
#define ICOD_TXB2           5
#define ICOD_RXB0           6
#define ICOD_RXB1           7

// CANCTRL
#define REQOP_MASK          (0x7 << 5)
#define REQOP(m)            ((m) << 5)  // Режимы такие же, как в регистре CANSTAT, поле OPMOD
#define ABAT                (1 << 4)
#define OSM                 (1 << 3)
#define CLKEN               (1 << 2)
#define CLKPRE_DIV1         (0 << 0)
#define CLKPRE_DIV2         (1 << 0)
#define CLKPRE_DIV4         (2 << 0)
#define CLKPRE_DIV8         (3 << 0)
#define CLKPRE_DIV_MASK     (3 << 0)

// CNF3
#define SOF                 (1 << 7)
#define WAKFIL              (1 << 6)
#define PHSEG2(n)           (n)

// CNF2
#define BTLMODE             (1 << 7)
#define SAM                 (1 << 6)
#define PHSEG1(n)           ((n) << 3)
#define PRSEG(n)            ((n) << 0)

// CNF1
#define SJW(n)              ((n) << 6)
#define BRP(n)              (n)

// CANINTE
#define MERRE               (1 << 7)
#define WAKIE               (1 << 6)
#define ERRIE               (1 << 5)
#define TXnIE(n)            ((1 << (n)) << 2)
#define RXnIE(n)            ((1 << (n)) << 0)

// CANINTF
#define MERRF               (1 << 7)
#define WAKIF               (1 << 6)
#define ERRIF               (1 << 5)
#define TXnIF(n)            ((1 << (n)) << 2)
#define RXnIF(n)            ((1 << (n)) << 0)

// EFLG
#define RXnOVR(n)           ((1 << (n)) << 6)
#define TXBO                (1 << 5)
#define TXEP                (1 << 4)
#define RXEP                (1 << 3)
#define TXWAR               (1 << 2)
#define RXWAR               (1 << 1)
#define EWARN               (1 << 0)

// TXBnCTRL
#define ABTF                (1 << 6)
#define MLOA                (1 << 5)
#define TXERR               (1 << 4)
#define TXREQ               (1 << 3)
#define TXP(n)              (n)
#define TXP_MASK            0x3
#define TXP_MIN             0
#define TXP_MAX             3

// RXB0CTRL, RXB1CTRL
#define RXM_OFF             (3 << 5)
#define RXM_EID             (2 << 5)
#define RXM_SID             (1 << 5)
#define RXM_SID_AND_EID     (0 << 5)
#define RXM_MASK            (3 << 5)
#define RXRTR               (1 << 3)
#define BUKT                (1 << 2)    // только RXB0CTRL
#define FILHIT0             (1 << 0)    // только RXB0CTRL
#define FILHIT1(r)          ((r) & 7)   // только RXB1CTRL


//
// Коды команд SPI
//
#define CMD_RESET           0xC0
#define CMD_READ            0x03
#define CMD_WRITE           0x02
#define CMD_RTS(mask)       (0x80 | (mask))
#define CMD_READ_STATUS     0xA0
#define CMD_RX_STATUS       0xB0
#define CMD_BIT_MODIFY      0x05
// Read RX Buffer - 4 возможных кода
#define CMD_READ_RXBnSIDH(n)    (0x90 + ((n) * 4))
#define CMD_READ_RXBnD0(n)      (0x92 + ((n) * 4))
// Load TX Buffer - 6 возможных кодов
#define CMD_LOAD_TXBnSIDH(n)    (0x40 + ((n) * 2))
#define CMD_LOAD_TXBnD0(n)      (0x41 + ((n) * 2))


#endif // __MCP2515_PRIVATE_H__
