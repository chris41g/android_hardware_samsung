#ifndef _LIBDDC_H_
#define _LIBDDC_H_

#ifdef __cplusplus
extern "C" {
#endif

int DDCOpen();
int DDCRead(unsigned char addr, unsigned char offset, unsigned int size, unsigned char* buffer);
int DDCWrite(unsigned char addr, unsigned char offset, unsigned int size, unsigned char* buffer);
int EDDCRead(unsigned char segpointer, unsigned char segment, unsigned char addr,
  unsigned char offset, unsigned int size, unsigned char* buffer);
int DDCClose();

#ifdef __cplusplus
}
#endif

#endif /* _LIBDDC_H_ */
