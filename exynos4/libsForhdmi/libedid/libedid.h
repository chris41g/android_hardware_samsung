#ifndef _LIBEDID_H_
#define _LIBEDID_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "video.h"
#include "audio.h"

int EDIDOpen();
int EDIDRead();
void EDIDReset();
/*
int EDIDHDMIModeSupport(const struct HDMIVideoParameter const *video);
int EDIDVideoResolutionSupport(const struct HDMIVideoParameter const *video);
int EDIDColorDepthSupport(const struct HDMIVideoParameter const *video);
int EDIDColorSpaceSupport(const struct HDMIVideoParameter const *video);
int EDIDColorimetrySupport(const struct HDMIVideoParameter const *video);
int EDIDAudioModeSupport(const struct HDMIAudioParameter const *audio);
int EDIDGetCECPhysicalAddress(int* const outAddr);
*/
int EDIDHDMIModeSupport( struct HDMIVideoParameter  *video);
int EDIDVideoResolutionSupport( struct HDMIVideoParameter  *video);
int EDIDColorDepthSupport( struct HDMIVideoParameter  *video);
int EDIDColorSpaceSupport( struct HDMIVideoParameter  *video);
int EDIDColorimetrySupport( struct HDMIVideoParameter  *video);
int EDIDAudioModeSupport( struct HDMIAudioParameter  *audio);
int EDIDGetCECPhysicalAddress(int*  outAddr);

int EDIDClose();

#ifdef __cplusplus
}
#endif
#endif /* _LIBEDID_H_ */
