#ifndef __S5PRP_API_H__
#define __S5PRP_API_H__

#include "s5p-rp_ioctl.h"

#ifdef __cplusplus
extern "C" {
#endif

int S5P_RP_Create(int block_mode);
int S5P_RP_Init(unsigned int ibuf_size);
int S5P_RP_Decode(void *buff, int size_byte);
int S5P_RP_Send_EOS(void);
int S5P_RP_Resume_EOS(void);
int S5P_RP_Pause(void);
int S5P_RP_Stop(void);
int S5P_RP_Flush(void);
int S5P_RP_SetParams(int id, unsigned long val);
int S5P_RP_GetParams(int id, unsigned long *pval);
int S5P_RP_Deinit(void);
int S5P_RP_Terminate(void);
int S5P_RP_IsOpen(void);

void S5P_RP_Set_Effect(int effect);    /* test only */
void S5P_RP_Enable_Effect(int on);
void S5P_RP_Set_Effect_Def(unsigned long effect_def);
void S5P_RP_Set_Effect_EQ_User(unsigned long eq_user);

#define S5P_RP_INIT_BLOCK_MODE                  0
#define S5P_RP_INIT_NONBLOCK_MODE               1

#define S5P_RP_PENDING_STATE_RUNNING            0
#define S5P_RP_PENDING_STATE_PENDING            1

#define S5P_RP_DEV_NAME                         "dev/s5p-rp"
#define S5P_RP_CTRL_DEV_NAME                    "dev/s5p-rp_ctrl"

#define S5P_RP_ERROR_LOSTSYNC                   0x00101
#define S5P_RP_ERROR_BADLAYER                   0x00102
#define S5P_RP_ERROR_BADBITRATE                 0x00103
#define S5P_RP_ERROR_BADSAMPLERATE              0x00104
#define S5P_RP_ERROR_BADEMPHASIS                0x00105

#define S5P_RP_ERROR_BADCRC                     0x00201
#define S5P_RP_ERROR_BADBITALLOC                0x00211
#define S5P_RP_ERROR_BADBADSCALEFACTOR          0x00221
#define S5P_RP_ERROR_BADFRAMELEN                0x00231
#define S5P_RP_ERROR_BADBIGVALUES               0x00232
#define S5P_RP_ERROR_BADBLOCKTYPE               0x00233
#define S5P_RP_ERROR_BADSCFSI                   0x00234
#define S5P_RP_ERROR_BADDATAPTR                 0x00235
#define S5P_RP_ERROR_BADPART3LEN                0x00236
#define S5P_RP_ERROR_BADHUFFTABLE               0x00237
#define S5P_RP_ERROR_BADHUFFDATA                0x00238
#define S5P_RP_ERROR_BADSTEREO                  0x00239

#ifdef __cplusplus
}
#endif

#endif //__S5PRP_API_H__
