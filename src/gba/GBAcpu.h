#ifndef GBACPU_H
#define GBACPU_H

#define UPDATE_REG(address, value)	WRITE16LE(((u16 *)&ioMem[address]),value)
#define READ_REG(address)           READ16LE((u16 *)&ioMem[address])
#define READ_SREG(address)          (*((s16 *)&ioMem[address]))
#define READ_U8REG(address)         ioMem[address]

#endif // GBACPU_H
