#ifndef DATATYPES_DEP_H_
#define DATATYPES_DEP_H_

/**
*\file
* \brief Implementation specific datatype

 */
typedef enum {FALSE=0, TRUE} Boolean;
typedef char Octet;
typedef signed char Integer8;
typedef signed short Integer16;
typedef signed int Integer32;
typedef unsigned char UInteger8;
typedef unsigned short UInteger16;
typedef unsigned int UInteger32;
typedef unsigned short Enumeration16;
typedef unsigned char Enumeration8;
typedef unsigned char Enumeration4;
typedef unsigned char UInteger4;
typedef unsigned char Nibble;

typedef unsigned int SOCKET;

/**
* \brief Implementation specific of UInteger48 type
 */
typedef struct {
	unsigned int lsb;
	unsigned short msb;
} UInteger48;

/**
* \brief Implementation specific of Integer64 type
 */
typedef struct {
	unsigned int lsb;
	int msb;
} Integer64;

/**
* \brief Struct used to average the offset from master
*
* The FIR filtering of the offset from master input is a simple, two-sample average
 */
typedef struct {
  Integer32  nsec_prev, y;
} offset_from_master_filter;

/**
* \brief Struct used to average the one way delay
*
* It is a variable cutoff/delay low-pass, infinite impulse response (IIR) filter.
*
*  The one-way delay filter has the difference equation: s*y[n] - (s-1)*y[n-1] = x[n]/2 + x[n-1]/2, where increasing the stiffness (s) lowers the cutoff and increases the delay.
 */
typedef struct {
  Integer32  nsec_prev, y;
  Integer32  s_exp;
} one_way_delay_filter;


#define TX_STACK_SIZE 1

struct tx_item {
  int len;
  struct timespec ts;
  unsigned char buf[PACKET_SIZE];
};

/**
* \brief Struct used to store network datas
 */
typedef struct {
  Integer32 eventSock, generalSock, multicastAddr, peerMulticastAddr,unicastAddr;
  Integer32 rawSock;
  UInteger32    rawIfIndex;             /**< Interface Index of raw socket */
  char          ifName[IFNAMSIZ];       /**< Interface name (e.g. "eth0") */
  unsigned char portMacAddress[6];      /**< Local Hardware Port MAC address */
  unsigned char rawDestAddress[6];      /**< Destination MAC Address for raw socket messages */
  unsigned char rawDestPDelayAddress[6];/**< Destination MAC Address for raw socket PDelay messages */
  int mtusize;
  Boolean hwTimestamping;
  struct {
    struct tx_item data[TX_STACK_SIZE];
    int count;
  } tx_stack;
} NetPath;

#endif /*DATATYPES_DEP_H_*/
