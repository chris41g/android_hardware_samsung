#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <fcntl.h>
#include <ctype.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#include "s5p-rp_api.h"
#include "s5p-rp_ioctl.h"

#define LOG_TAG "libs5prp"
#include <cutils/log.h>

/* Disable LOGD message */
#ifdef LOGD
#undef LOGD
#endif
#define LOGD(...)

//#define _USE_WBUF_            /* Buffering before writing srp-rp device */
//#define _DUMP_TO_FILE_
//#define _USE_FW_FROM_DISK_

#ifdef _USE_WBUF_
/* WriteBuffer Size = IBUF size * WBUF_LEN_MUL */
#define WBUF_LEN_MUL        2
#endif

static int s5p_rp_dev = -1;
static int s5p_rp_ibuf_size = 0;
static int s5p_rp_block_mode = S5P_RP_INIT_BLOCK_MODE;

static unsigned char *wbuf;
static int wbuf_size;
static int wbuf_pos;

#ifdef _DUMP_TO_FILE_
static FILE *fp_dump = NULL;
#endif

#ifdef _USE_FW_FROM_DISK_
static char s5p_alt_fw_name_pre[6][32] = {
    "sdcard/rp_fw/rp_fw_code1",
    "sdcard/rp_fw/rp_fw_code20",
    "sdcard/rp_fw/rp_fw_code21",
    "sdcard/rp_fw/rp_fw_code22",
    "sdcard/rp_fw/rp_fw_code30",
    "sdcard/rp_fw/rp_fw_code31",
};
#endif

#ifdef _USE_WBUF_
static int WriteBuff_Init(void)
{
    if (wbuf == NULL) {
        wbuf_size = s5p_rp_ibuf_size * WBUF_LEN_MUL;
        wbuf_pos = 0;
        wbuf = (unsigned char *)malloc(wbuf_size);
        LOGD("%s: WriteBuffer %dbytes allocated", __func__, wbuf_size);
        return 0;
    }

    LOGE("%s: WriteBuffer already allocated", __func__);
    return -1;
}

static int WriteBuff_Deinit(void)
{
    if (wbuf != NULL) {
        free(wbuf);
        wbuf = NULL;
        return 0;
    }

    LOGE("%s: WriteBuffer is not ready", __func__);
    return -1;
}

static int WriteBuff_Write(unsigned char *buff, int size_byte)
{
    int write_byte;

    if ((wbuf_pos + size_byte) < wbuf_size) {
        memcpy(&wbuf[wbuf_pos], buff, size_byte);
        wbuf_pos += size_byte;
    } else {
        LOGE("%s: WriteBuffer is filled [%d], ignoring write [%d]", __func__, wbuf_pos, size_byte);
        return -1;    /* Insufficient buffer */
    }

    return wbuf_pos;
}

static void WriteBuff_Consume(void)
{
    memcpy(wbuf, &wbuf[s5p_rp_ibuf_size], s5p_rp_ibuf_size * (WBUF_LEN_MUL - 1));
    wbuf_pos -= s5p_rp_ibuf_size;
}

static void WriteBuff_Flush(void)
{
    wbuf_pos = 0;
}
#endif

#ifdef _USE_FW_FROM_DISK_
/* This will check & download alternate firmware */
static int S5P_RP_Check_AltFirmware(void)
{
    unsigned long *temp_buff;
    FILE *fp = NULL;

    int s5p_rp_ctrl = -1;
    char alt_fw_name[128];
    unsigned long alt_fw_set;
    unsigned long alt_fw_loaded = 0;
    int alt_fw_text_ok,alt_fw_data_ok;

    if ((s5p_rp_ctrl = open(S5P_RP_CTRL_DEV_NAME, O_RDWR | O_NDELAY)) >= 0) {
        ioctl(s5p_rp_ctrl, S5P_RP_CTRL_ALTFW_STATE, &alt_fw_loaded);

        if (!alt_fw_loaded) {    /* Not loaded yet? */
            LOGE("Try to download alternate RP firmware");
            temp_buff = (unsigned long *)malloc(256*1024);    /* temp buffer */

            for (alt_fw_set = 0; alt_fw_set < 6; alt_fw_set++) {
                sprintf(alt_fw_name, "%s_text.bin", s5p_alt_fw_name_pre[alt_fw_set]);
                if (fp = fopen(alt_fw_name, "rb")) {
                    LOGE("RP Alt-Firmware Loading: %s", alt_fw_name);
                    fread(temp_buff, 64*1024, 1, fp);
                    close(fp);
                    alt_fw_text_ok = 1;
                } else {
                    alt_fw_text_ok = 0;
                }

                sprintf(alt_fw_name, "%s_data.bin", s5p_alt_fw_name_pre[alt_fw_set]);
                if (fp = fopen(alt_fw_name, "rb")) {
                    LOGE("RP Alt-Firmware Loading: %s", alt_fw_name);
                    fread(&temp_buff[64*1024/4], 96*1024, 1, fp);
                    close(fp);
                    alt_fw_data_ok = 1;
                } else {
                    alt_fw_data_ok = 0;
                }

                if (alt_fw_text_ok && alt_fw_data_ok) {
                    temp_buff[160*1024/4] = alt_fw_set;
                    ioctl(s5p_rp_ctrl, S5P_RP_CTRL_ALTFW_LOAD, temp_buff);
                }
            }
            free(temp_buff);
        }
        close(s5p_rp_ctrl);
    }

    return 0;
}
#endif

int S5P_RP_Create(int block_mode)
{
    if (s5p_rp_dev == -1) {
#ifdef _USE_FW_FROM_DISK_
        S5P_RP_Check_AltFirmware();
#endif

        s5p_rp_block_mode = block_mode;
        s5p_rp_dev = open(S5P_RP_DEV_NAME, O_RDWR |
                    ((block_mode == S5P_RP_INIT_NONBLOCK_MODE) ? O_NDELAY : 0));

        return s5p_rp_dev;
    }

    LOGE("%s: Device is not ready", __func__);
    return -1;    /* device alreay opened */
}

int S5P_RP_Init(unsigned int ibuf_size)
{
    int ret;

    if (s5p_rp_dev != -1) {
        s5p_rp_ibuf_size = ibuf_size;
        ret = ioctl(s5p_rp_dev, S5P_RP_INIT, s5p_rp_ibuf_size); /* Initialize IBUF size (4KB ~ 18KB) */

#ifdef _DUMP_TO_FILE_
        char outname[256];
        int cnt = 0;

        while (1) {
            sprintf(outname, "/data/rp_dump_%04d.mp3", cnt++);
            if (fp_dump = fopen(outname, "rb")) { /* file exist? */
                fclose(fp_dump);
            } else {
                break;
            }
        }

        LOGD("%s: Dump MP3 to %s", __func__, outname);
        if (fp_dump = fopen(outname, "wb")) {
            LOGD("%s: Success to open %s", __func__, outname);
        } else {
            LOGD("%s: Fail to open %s", __func__, outname);
        }
#endif

#ifdef _USE_WBUF_
        if (ret != -1) {
            return WriteBuff_Init();
        }
#else
        return ret;
#endif
    }

    LOGE("%s: Device is not ready", __func__);
    return -1;  /* device is not created */
}

#ifdef _USE_WBUF_
int S5P_RP_Decode(void *buff, int size_byte)
{
    int ret;
    int val;
    int err_code = 0;

    if (s5p_rp_dev != -1) {
        /* Check wbuf before writing buff */
        while (wbuf_pos >= s5p_rp_ibuf_size) { /* Write_Buffer filled? (IBUF Size)*/
            LOGD("%s: Write Buffer is full, Send data to RP", __func__);

            ret = write(s5p_rp_dev, wbuf, s5p_rp_ibuf_size); /* Write Buffer to RP Driver */
            if (ret == -1) { /* Fail? */
                ioctl(s5p_rp_dev, S5P_RP_ERROR_STATE, &val);
                if (!val) {    /* Write error? */
                    LOGE("%s: IBUF write fail", __func__);
                    return -1;
                } else {       /* Write OK, but RP decode error? */
                    err_code = val;
                    LOGE("%s: RP decode error [0x%05X]", __func__, err_code);
                }
            }
#ifdef _DUMP_TO_FILE_
            if (fp_dump) {
                fwrite(wbuf, s5p_rp_ibuf_size, 1, fp_dump);
            }
#endif
            WriteBuff_Consume();
        }

        ret = WriteBuff_Write((unsigned char *)buff, size_byte);
        if (ret == -1) {
            return -1;  /* Buffering error */
        }

        LOGD("%s: Write Buffer remain [%d]", __func__, wbuf_pos);
        return err_code;  /* Write Success */
    }

    LOGE("%s: Device is not ready", __func__);
    return -1;  /* device is not created */
}

int S5P_RP_Send_EOS(void)
{
    int ret;
    int val;

    if (s5p_rp_dev != -1) {
        /* Check wbuf before writing buff */
        while (wbuf_pos) { /* Write_Buffer ramain?*/
            if (wbuf_pos < s5p_rp_ibuf_size) {
                memset(wbuf + wbuf_pos, 0xFF, s5p_rp_ibuf_size - wbuf_pos); /* Fill dummy data */
                wbuf_pos = s5p_rp_ibuf_size;
            }

            ret = write(s5p_rp_dev, wbuf, s5p_rp_ibuf_size); /* Write Buffer to RP Driver */
            if (ret == -1) {  /* Fail? */
                ret = ioctl(s5p_rp_dev, S5P_RP_ERROR_STATE, &val);
                if (!val) {   /* Write error? */
                    LOGE("%s: IBUF write fail", __func__);
                    return -1;
                } else {      /* RP decoe error? */
                    LOGE("%s: RP decode error [0x%05X]", __func__, val);
                    return -1;
                }
            } else {          /* Success? */
#ifdef _DUMP_TO_FILE_
                if (fp_dump) {
                    fwrite(wbuf, s5p_rp_ibuf_size, 1, fp_dump);
                }
#endif
                WriteBuff_Consume();
            }
        }

        memset(wbuf, 0xFF, s5p_rp_ibuf_size);      /* Fill dummy data */
        write(s5p_rp_dev, wbuf, s5p_rp_ibuf_size); /* Write Buffer to RP Driver */

        /* Wait until RP decoding over */
        return ioctl(s5p_rp_dev, S5P_RP_WAIT_EOS);
    }

    return -1; /* device is not created */
}
#else  /* Without WBUF */
int S5P_RP_Decode(void *buff, int size_byte)
{
    int ret;
    int val;
    int err_code = 0;

    if (s5p_rp_dev != -1) {
        LOGD("%s: Send data to RP (%d bytes)", __func__, size_byte);

        ret = write(s5p_rp_dev, buff, size_byte);  /* Write Buffer to RP Driver */
        if (ret == -1) {  /* Fail? */
            ioctl(s5p_rp_dev, S5P_RP_ERROR_STATE, &val);
            if (!val) {   /* Write error? */
                LOGE("%s: IBUF write fail", __func__);
                return -1;
            } else {      /* Write OK, but RP decode error? */
                err_code = val;
                LOGE("%s: RP decode error [0x%05X]", __func__, err_code);
            }
        }
#ifdef _DUMP_TO_FILE_
        if (fp_dump) {
            fwrite(buff, size_byte, 1, fp_dump);
        }
#endif

        return err_code; /* Write Success */
    }

    LOGE("%s: Device is not ready", __func__);
    return -1; /* device is not created */
}

int S5P_RP_Send_EOS(void)
{
    if (s5p_rp_dev != -1) {
        /* Wait until RP decoding over */
        return ioctl(s5p_rp_dev, S5P_RP_SEND_EOS);
    }

    return -1; /* device is not created */
}

int S5P_RP_Resume_EOS(void)
{
    if (s5p_rp_dev != -1) {
        return ioctl(s5p_rp_dev, S5P_RP_RESUME_EOS);
    }

    return -1; /* device is not created */
}
#endif

int S5P_RP_Pause(void)
{
    if (s5p_rp_dev != -1) {
        return ioctl(s5p_rp_dev, S5P_RP_PAUSE);
    }

    return -1; /* device is not created */
}

int S5P_RP_Stop(void)
{
    if (s5p_rp_dev != -1) {
        return ioctl(s5p_rp_dev, S5P_RP_STOP);
    }

    return -1; /* device is not created */
}

int S5P_RP_Flush(void)
{
    if (s5p_rp_dev != -1) {
        if (ioctl(s5p_rp_dev, S5P_RP_FLUSH) != -1) {
#ifdef _USE_WBUF_
            WriteBuff_Flush();
#endif
            return 0;
        }
    }

    return -1; /* device is not created */
}

void S5P_RP_Set_Effect(int effect)
{
    int s5p_rp_ctrl = -1;
    unsigned long effect_mode = (unsigned long)effect;

    if ((s5p_rp_ctrl = open(S5P_RP_CTRL_DEV_NAME, O_RDWR | O_NDELAY)) >= 0) {
        ioctl(s5p_rp_ctrl, S5P_RP_CTRL_SET_EFFECT, effect_mode);
        close(s5p_rp_ctrl);
    }
}

void S5P_RP_Enable_Effect(int on)
{
    int s5p_rp_ctrl = -1;
    unsigned long effect_switch = on ? 1 : 0;

    if ((s5p_rp_ctrl = open(S5P_RP_CTRL_DEV_NAME, O_RDWR | O_NDELAY)) >= 0) {
        ioctl(s5p_rp_ctrl, S5P_RP_CTRL_EFFECT_ENABLE, effect_switch);
        close(s5p_rp_ctrl);
    }
}

void S5P_RP_Set_Effect_Def(unsigned long effect_def)
{
    int s5p_rp_ctrl = -1;

    if ((s5p_rp_ctrl = open(S5P_RP_CTRL_DEV_NAME, O_RDWR | O_NDELAY)) >= 0) {
        ioctl(s5p_rp_ctrl, S5P_RP_CTRL_EFFECT_DEF, effect_def);
        close(s5p_rp_ctrl);
    }
}

void S5P_RP_Set_Effect_EQ_User(unsigned long eq_user)
{
    int s5p_rp_ctrl = -1;

    if ((s5p_rp_ctrl = open(S5P_RP_CTRL_DEV_NAME, O_RDWR | O_NDELAY)) >= 0) {
        ioctl(s5p_rp_ctrl, S5P_RP_CTRL_EFFECT_EQ_USR, eq_user);
        close(s5p_rp_ctrl);
    }
}

int S5P_RP_SetParams(int id, unsigned long val)
{
    if (s5p_rp_dev != -1)
        return 0; /* not yet */

    return -1;    /* device is not created */
}

int S5P_RP_GetParams(int id, unsigned long *pval)
{
    if (s5p_rp_dev != -1)
        return ioctl(s5p_rp_dev, id, pval);

    return -1;    /* device is not created */
}

int S5P_RP_Deinit(void)
{
    if (s5p_rp_dev != -1) {
#ifdef _DUMP_TO_FILE_
        if (fp_dump) {
            fclose(fp_dump);
        }
#endif

#ifdef _USE_WBUF_
        WriteBuff_Deinit();
#endif
        return ioctl(s5p_rp_dev, S5P_RP_DEINIT); /* Deinialize */
    }

    LOGE("%s: Device is not ready", __func__);
    return -1;    /* device is not created */
}

int S5P_RP_Terminate(void)
{
    int ret;

    if (s5p_rp_dev != -1) {
        ret = close(s5p_rp_dev);

        if (ret == 0) {
            s5p_rp_dev = -1; /* device closed */
            return 0;
        }
    }

    LOGE("%s: Device is not ready", __func__);
    return -1; /* device is not created or close error*/
}

int S5P_RP_IsOpen(void)
{
    if (s5p_rp_dev == -1) {
        LOGD("%s: Device is not opened", __func__);
        return 0;
    }

    LOGD("%s: Device is opened", __func__);
    return 1;
}
