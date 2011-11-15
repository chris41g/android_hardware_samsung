/*
 * copyright@ samsung electronics co. ltd
 *
 * licensed under the apache license, version 2.0 (the "license");
 * you may not use this file except in compliance with the license.
 * you may obtain a copy of the license at
 *
 *      http://www.apache.org/licenses/license-2.0
 *
 * unless required by applicable law or agreed to in writing, software
 * distributed under the license is distributed on an "as is" basis,
 * without warranties or conditions of any kind, either express or implied.
 * see the license for the specific language governing permissions and
 * limitations under the license.
 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <cutils/log.h>

#include "edid.h"
#include "libedid.h"
//#include "../libddc/libddc.h"
#include "libddc.h"

//#define EDID_DEBUG 1

#ifdef EDID_DEBUG
#define DPRINTF(args...)    LOGI(args)
#else
#define DPRINTF(args...)
#endif

/**
 * @var gEdidData
 * Pointer to EDID data
 */
static unsigned char* gEdidData;

/**
 * @var gExtensions
 * Number of EDID extensions
 */
static int gExtensions;

//! Structure for parsing video timing parameter in EDID
static const struct edid_params
{
    /** H Blank */
    unsigned int vHBlank;
    /** V Blank */
    unsigned int vVBlank;
    /**
     * H Total + V Total @n
     * For more information, refer HDMI register map
     */
    unsigned int vHVLine;
    /** CEA VIC */
    unsigned char  VIC;
    /** CEA VIC for 16:9 aspect ratio */
    unsigned char  VIC16_9;
    /** 0 if progressive, 1 if interlaced */
    unsigned char  interlaced;
    /** Pixel frequency */
    enum PixelFreq PixelClock;
} aVideoParams[] =
{
    { 0xA0 , 0x16A0D, 0x32020D,  1 , 1 , 0, PIXEL_FREQ_25_200,  },  // v640x480p_60Hz
    { 0x8A , 0x16A0D, 0x35A20D,  2 , 3 , 0, PIXEL_FREQ_27_027,  },  // v720x480p_60Hz
    { 0x172, 0xF2EE , 0x6722EE,  4 , 4 , 0, PIXEL_FREQ_74_250,  },  // v1280x720p_60Hz
    { 0x118, 0xB232 , 0x898465,  5 , 5 , 1, PIXEL_FREQ_74_250,  },  // v1920x1080i_60Hz
    { 0x114, 0xB106 , 0x6B420D,  6 , 7 , 1, PIXEL_FREQ_27_027,  },  // v720x480i_60Hz
    { 0x114, 0xB106 , 0x6B4106,  8 , 9 , 0, PIXEL_FREQ_27_027,  },  // v720x240p_60Hz
    { 0x228, 0xB106 , 0xD6820D,  10, 11, 1, PIXEL_FREQ_54_054,  },  // v2880x480i_60Hz
    { 0x228, 0xB106 , 0x6B4106,  12, 13, 0, PIXEL_FREQ_54_054,  },  // v2880x240p_60Hz
    { 0x114, 0x16A0D, 0x6B420D,  14, 15, 0, PIXEL_FREQ_54_054,  },  // v1440x480p_60Hz
    { 0x118, 0x16C65, 0x898465,  16, 16, 0, PIXEL_FREQ_148_500, },  // v1920x1080p_60Hz
    { 0x90 , 0x18A71, 0x360271,  17, 18, 0, PIXEL_FREQ_27,      },  // v720x576p_50Hz
    { 0x2BC, 0xF2EE , 0x7BC2EE,  19, 19, 0, PIXEL_FREQ_74_250,  },  // v1280x720p_50Hz
    { 0x2D0, 0xB232 , 0xA50465,  20, 20, 1, PIXEL_FREQ_74_250,  },  // v1920x1080i_50Hz
    { 0x120, 0xC138 , 0x6C0271,  21, 22, 1, PIXEL_FREQ_27,      },  // v720x576i_50Hz
    { 0x120, 0xC138 , 0x6C0138,  23, 24, 0, PIXEL_FREQ_27,      },  // v720x288p_50Hz
    { 0x240, 0xC138 , 0xD80271,  25, 26, 1, PIXEL_FREQ_54,      },  // v2880x576i_50Hz
    { 0x240, 0xC138 , 0xD80138,  27, 28, 0, PIXEL_FREQ_54,      },  // v2880x288p_50Hz
    { 0x120, 0x18A71, 0x6C0271,  29, 30, 0, PIXEL_FREQ_54,      },  // v1440x576p_50Hz
    { 0x2D0, 0x16C65, 0xA50465,  31, 31, 0, PIXEL_FREQ_148_500, },  // v1920x1080p_50Hz
    { 0x33E, 0x16C65, 0xABE465,  32, 32, 0, PIXEL_FREQ_74_250,  },  // v1920x1080p_24Hz
    { 0x2D0, 0x16C65, 0xA50465,  33, 33, 0, PIXEL_FREQ_74_250,  },  // v1920x1080p_25Hz
    { 0x2D0, 0x16C65, 0xA50465,  34, 34, 0, PIXEL_FREQ_74_250,  },  // v1920x1080p_30Hz
    { 0x228, 0x16A0D, 0xD6820D,  35, 36, 0, PIXEL_FREQ_108_108, },  // v2880x480p_60Hz
    { 0x240, 0x18A71, 0xD80271,  37, 38, 0, PIXEL_FREQ_108,     },  // v2880x576p_50Hz
    { 0x180, 0x2AA71, 0x9004E2,  39, 39, 0, PIXEL_FREQ_72,      },  // v1920x1080i_50Hz(1250)
    { 0x2D0, 0xB232 , 0xA50465,  40, 40, 1, PIXEL_FREQ_148_500, },  // v1920x1080i_100Hz
    { 0x2BC, 0xF2EE , 0x7BC2EE,  41, 41, 0, PIXEL_FREQ_148_500, },  // v1280x720p_100Hz
    { 0x90 , 0x18A71, 0x360271,  42, 43, 0, PIXEL_FREQ_54,      },  // v720x576p_100Hz
    { 0x120, 0xC138 , 0x6C0271,  44, 45, 1, PIXEL_FREQ_54,      },  // v720x576i_100Hz
    { 0x118, 0xB232 , 0x898465,  46, 46, 1, PIXEL_FREQ_148_500, },  // v1920x1080i_120Hz
    { 0x172, 0xF2EE , 0x6722EE,  47, 47, 0, PIXEL_FREQ_148_500, },  // v1280x720p_120Hz
    { 0x8A , 0x16A0D, 0x35A20D,  48, 49, 0, PIXEL_FREQ_54_054,  },  // v720x480p_120Hz
    { 0x114, 0xB106 , 0x6B420D,  50, 51, 1, PIXEL_FREQ_54_054,  },  // v720x480i_120Hz
    { 0x90 , 0x18A71, 0x360271,  52, 53, 0, PIXEL_FREQ_108,     },  // v720x576p_200Hz
    { 0x120, 0xC138 , 0x6C0271,  54, 55, 1, PIXEL_FREQ_108,     },  // v720x576i_200Hz
    { 0x8A , 0x16A0D, 0x35A20D,  56, 57, 0, PIXEL_FREQ_108_108, },  // v720x480p_240Hz
    { 0x114, 0xB106 , 0x6B420D,  58, 59, 1, PIXEL_FREQ_108_108, },  // v720x480i_240Hz
};


/**
 * Calculate a checksum.
 * @param   buffer  [in]    Pointer to data to calculate a checksum
 * @param   size    [in]    Sizes of data
 * @return  If checksum result is 0, return 1; Otherwise, return 0.
 */
static int CalcChecksum(const unsigned char* const buffer, const int size)
{
    unsigned char i,sum;
    int ret = 1;

    // check parameter
    if ( buffer == NULL ){
        DPRINTF("invalid parameter : buffer\n");
        return 0;
    }
    for (sum = 0, i = 0 ; i < size; i++)
    {
        sum += buffer[i];
    }

    // check checksum
    if (sum != 0)
    {
        ret = 0;
    }

    return ret;
}

/**
 * Read EDID Block(128 bytes)
 * @param   blockNum    [in]    Number of block to read
 * @param   outBuffer   [out]   Pointer to buffer to store EDID data
 * @return  If fail to read, return 0; Otherwise, return 1.
 */
static int ReadEDIDBlock(const unsigned int blockNum, unsigned char* const outBuffer)
{
    int segNum, offset, dataPtr;

    // check parameter
    if (outBuffer == NULL){
        DPRINTF("invalid parameter : outBuffer\n");
        return 0;
    }

    // calculate
    segNum = blockNum / 2;
    offset = (blockNum % 2) * SIZEOFEDIDBLOCK;
    dataPtr = (blockNum) * SIZEOFEDIDBLOCK;

    // read block
    if ( !EDDCRead(EDID_SEGMENT_POINTER, segNum, EDID_ADDR, offset, SIZEOFEDIDBLOCK, outBuffer) )
    {
        DPRINTF("Fail to Read %dth EDID Block\n", blockNum);
        return 0;
    }

    if (!CalcChecksum(outBuffer, SIZEOFEDIDBLOCK))
    {
        DPRINTF("CheckSum fail : %dth EDID Block\n", blockNum);
        return 0;
    }

    // print data
#ifdef EDID_DEBUG
    offset = 0;
    do
    {
        LOGI("0x%02X", outBuffer[offset++]);
    }
    while (SIZEOFEDIDBLOCK > offset);
#endif
    return 1;
}

/**
 * Check if EDID data is valid or not.
 * @return if EDID data is valid, return 1; Otherwise, return 0.
 */
static inline int EDIDValid(void)
{
    return (gEdidData == NULL) ?  0 : 1;
}

/**
 * Check if EDID extension block is timing extension block or not.
 * @param   extension   [in] The number of EDID extension block to check
 * @return  If the block is timing extension, return 1; Otherwise, return 0.
 */
static int IsTimingExtension(const int extension)
{
    if (!EDIDValid() || (extension > gExtensions) ){
        DPRINTF("EDID Data is not available\n");
        return 0;
    }

    if (gEdidData[extension*SIZEOFEDIDBLOCK] == EDID_TIMING_EXT_TAG_VAL
      && gEdidData[extension*SIZEOFEDIDBLOCK + EDID_TIMING_EXT_REV_NUMBER_POS] >= 1)
        return 1;
    return 0;
}

/**
 * Search Vender Specific Data Block(VSDB) in EDID extension block.
 * @param   extension   [in] the number of EDID extension block to check
 * @return  if there is a VSDB, return the its offset from start of @n
 *          EDID extension block. if there is no VSDB, return 0.
 */
static int GetVSDBOffset(const int extension)
{
    unsigned int BlockOffset = extension*SIZEOFEDIDBLOCK;
    unsigned int offset = BlockOffset + EDID_DATA_BLOCK_START_POS;
    unsigned int tag,blockLen,DTDOffset;

    if (!EDIDValid() || (extension > gExtensions) ){
        DPRINTF("EDID Data is not available\n");
        return 0;
    }

    DTDOffset = gEdidData[BlockOffset + EDID_DETAILED_TIMING_OFFSET_POS];

    // check if there is HDMI VSDB
    while ( offset < BlockOffset + DTDOffset )
    {
        // find the block tag and length
        // tag
        tag = gEdidData[offset] & EDID_TAG_CODE_MASK;
        // block len
        blockLen = (gEdidData[offset] & EDID_DATA_BLOCK_SIZE_MASK) + 1;

        // check if it is HDMI VSDB
        // if so, check identifier value, if it's hdmi vsbd - return offset
        if (tag == EDID_VSDB_TAG_VAL &&
            gEdidData[offset+1] == 0x03 &&
            gEdidData[offset+2] == 0x0C &&
            gEdidData[offset+3] == 0x0 &&
            blockLen > EDID_VSDB_MIN_LENGTH_VAL )
        {
            return offset;
        }
        // else find next block
        offset += blockLen;
    } // while()

    // return error
    return 0;
}

/**
 * Check if the video format is contained in - @n
 * Detailed Timing Descriptor(DTD) of EDID extension block.
 * @param   extension   [in]    Number of EDID extension block to check
 * @param   videoFormat [in]    Video format to check
 * @return  If the video format is contained in DTD of EDID extension block, -@n
 *          return 1; Otherwise, return 0.
 */
static int IsContainVideoDTD(const int extension,const enum VideoFormat videoFormat)
{
    int i,StartAddr,EndAddr;

    if (!EDIDValid() || (extension > gExtensions) ){
        DPRINTF("EDID Data is not available\n");
        return 0;
    }

    // if edid block( 0th block )
    if (extension == 0)
    {
        StartAddr = EDID_DTD_START_ADDR;
        EndAddr = StartAddr + EDID_DTD_TOTAL_LENGTH;
    }
    else // if edid extension block
    {
        StartAddr = extension*SIZEOFEDIDBLOCK + gEdidData[extension*SIZEOFEDIDBLOCK + EDID_DETAILED_TIMING_OFFSET_POS];
        EndAddr = (extension+1)*SIZEOFEDIDBLOCK;
    }

    // check DTD(Detailed Timing Description)
    for (i = StartAddr; i < EndAddr; i+= EDID_DTD_BYTE_LENGTH)
    {
        int hblank = 0, hactive = 0, vblank = 0, vactive = 0, interlaced = 0, pixelclock = 0;
        int vHActive = 0, vVActive = 0, vVBlank;

        // get pixel clock
        pixelclock = (gEdidData[i+EDID_DTD_PIXELCLOCK_POS2] << SIZEOFBYTE);
        pixelclock |= gEdidData[i+EDID_DTD_PIXELCLOCK_POS1];

        if (!pixelclock)
        {
            continue;
        }

        // get HBLANK value in pixels
        hblank = gEdidData[i+EDID_DTD_HBLANK_POS2] & EDID_DTD_HBLANK_POS2_MASK;
        hblank <<= SIZEOFBYTE; // lower 4 bits
        hblank |= gEdidData[i+EDID_DTD_HBLANK_POS1];

        // get HACTIVE value in pixels
        hactive = gEdidData[i+EDID_DTD_HACTIVE_POS2] & EDID_DTD_HACTIVE_POS2_MASK;
        hactive <<= (SIZEOFBYTE/2); // upper 4 bits
        hactive |= gEdidData[i+EDID_DTD_HACTIVE_POS1];

        // get VBLANK value in pixels
        vblank = gEdidData[i+EDID_DTD_VBLANK_POS2] & EDID_DTD_VBLANK_POS2_MASK;
        vblank <<= SIZEOFBYTE; // lower 4 bits
        vblank |= gEdidData[i+EDID_DTD_VBLANK_POS1];

        // get VACTIVE value in pixels
        vactive = gEdidData[i+EDID_DTD_VACTIVE_POS2] & EDID_DTD_VACTIVE_POS2_MASK;
        vactive <<= (SIZEOFBYTE/2); // upper 4 bits
        vactive |= gEdidData[i+EDID_DTD_VACTIVE_POS1];

        vHActive = ((aVideoParams[videoFormat].vHVLine & 0xFFF000)>>12) - aVideoParams[videoFormat].vHBlank;
        vVActive = (aVideoParams[videoFormat].vVBlank & 0x3FF);
        vVBlank = (aVideoParams[videoFormat].vVBlank & 0xFFC00) >> 11;
        vVActive -= vVBlank;

        // get Interlaced Mode Value
        interlaced = (int)(gEdidData[i+EDID_DTD_INTERLACE_POS] & EDID_DTD_INTERLACE_MASK);
        if (interlaced) interlaced = 1;

        if (hblank == aVideoParams[videoFormat].vHBlank && vblank == vVBlank // blank
            && hactive == vHActive && vactive == vVActive ) //line
        {
            int EDIDpixelclock = aVideoParams[videoFormat].PixelClock;
            EDIDpixelclock /= 100; pixelclock /= 100;

            if (pixelclock == EDIDpixelclock)
            {
                DPRINTF("Sink Support the Video mode\n");
                return 1;
            }
        }
    } // for
    return 0;
}

/**
 * Check if a VIC(Video Identification Code) is contained in -@n
 * EDID extension block.
 * @param   extension   [in]    Number of EDID extension block to check
 * @param   VIC         [in]    VIC to check
 * @return  If the VIC is contained in contained in EDID extension block, -@n
 *          return 1; Otherwise, return 0.
 */
static int IsContainVIC(const int extension, const int VIC)
{
    unsigned int StartAddr = extension*SIZEOFEDIDBLOCK;
    unsigned int ExtAddr = StartAddr + EDID_DATA_BLOCK_START_POS;
    unsigned int tag,blockLen;
    unsigned int DTDStartAddr = gEdidData[StartAddr + EDID_DETAILED_TIMING_OFFSET_POS];

    if (!EDIDValid() || (extension > gExtensions)){
        DPRINTF("EDID Data is not available\n");
        return 0;
    }

    // while
    while ( ExtAddr < StartAddr + DTDStartAddr )
    {
        // find the block tag and length
        // tag
        tag = gEdidData[ExtAddr] & EDID_TAG_CODE_MASK;
        // block len
        blockLen = (gEdidData[ExtAddr] & EDID_DATA_BLOCK_SIZE_MASK) + 1;
        DPRINTF("tag = %d\n",tag);
        DPRINTF("blockLen = %d\n",blockLen-1);

        // check if it is short video description
        if (tag == EDID_SHORT_VID_DEC_TAG_VAL)
        {
            // if so, check SVD
            int index;
            for (index=1; index < blockLen;index++)
            {
                DPRINTF("EDIDVIC = %d\n",gEdidData[ExtAddr+index] & EDID_SVD_VIC_MASK);
                DPRINTF("VIC = %d\n",VIC);

                // check VIC with SVDB
                if (VIC == (gEdidData[ExtAddr+index] & EDID_SVD_VIC_MASK) )
                {
                    DPRINTF("Sink Device supports requested video mode\n");
                    return 1;
                }
            } // for
        } // if tag
        // else find next block
        ExtAddr += blockLen;
    } // while()

    return 0;
}

/**
 * Check if EDID contains the video format.
 * @param   videoFormat [in]    Video format to check
 * @param   pixelRatio  [in]    Pixel aspect ratio of video format to check
 * @return  if EDID contains the video format, return 1; Otherwise, return 0.
 */
static int CheckResolution(const enum VideoFormat videoFormat,
                            const enum PixelAspectRatio pixelRatio)
{
    int index,vic;

    // read EDID
    if(!EDIDRead())
    {
        return 0;
    }

    // check ET(Established Timings) for 640x480p@60Hz
    if ( videoFormat == v640x480p_60Hz // if it's 640x480p@60Hz
        && (gEdidData[EDID_ET_POS] & EDID_ET_640x480p_VAL) ) // it support
    {
         return 1;
    }

    // check STI(Standard Timing Identification)
    // do not need

    // check DTD(Detailed Timing Description) of EDID block(0th)
    if (IsContainVideoDTD(0,videoFormat))
    {
        return 1;
    }

    // check EDID Extension
    vic = (pixelRatio == HDMI_PIXEL_RATIO_16_9) ? aVideoParams[videoFormat].VIC16_9 : aVideoParams[videoFormat].VIC;

    // find VSDB
    for ( index = 1 ; index <= gExtensions ; index++ )
    {
        if ( IsTimingExtension(index) ) // if it's timing block
        {
            if (IsContainVIC(index,vic) || IsContainVideoDTD(index,videoFormat))
                return 1;
        }
    }

    return 0;
}

/**
 * Check if EDID supports the color depth.
 * @param   depth [in]    Color depth
 * @param   space [in]    Color space
 * @return  If EDID supports the color depth, return 1; Otherwise, return 0.
 */
static int CheckColorDepth(const enum ColorDepth depth,const enum ColorSpace space)
{
    int index;
    unsigned int StartAddr;

    // if color depth == 24 bit, no need to check
    if ( depth == HDMI_CD_24 )
        return 1;

    // check EDID data is valid or not
    // read EDID
    if(!EDIDRead())
    {
        return 0;
    }

    // find VSDB
    for ( index = 1 ; index <= gExtensions ; index++ )
    {
        if ( IsTimingExtension(index) // if it's timing block
            && ((StartAddr = GetVSDBOffset(index)) > 0) )   // check block
        {
            int blockLength = gEdidData[StartAddr] & EDID_DATA_BLOCK_SIZE_MASK;
            if (blockLength >= EDID_DC_POS)
            {
                // get supported DC value
                int deepColor = gEdidData[StartAddr + EDID_DC_POS] & EDID_DC_MASK;
                DPRINTF("EDID deepColor = %x\n",deepColor);
                // check supported DeepColor

                // if YCBCR444
                if (space == HDMI_CS_YCBCR444)
                {
                    if ( !(deepColor & EDID_DC_YCBCR_VAL))
                        return 0;
                }

                // check colorDepth
                switch (depth)
                {
                    case HDMI_CD_36:
                    {
                        deepColor &= EDID_DC_36_VAL;
                        break;
                    }
                    case HDMI_CD_30:
                    {
                        deepColor &= EDID_DC_30_VAL;
                        break;
                    }
                    case HDMI_CD_24:
                    {
                        deepColor = 1;
                        break;
                    }
                    default :
                        deepColor = 0;
                } // switch

                if (deepColor)
                    return 1;
                else
                    return 0;
            } // if
        } // if
    } // for

    return 0;
}

/**
 * Check if EDID supports the color space.
 * @param   space [in]    Color space
 * @return  If EDID supports the color space, return 1; Otherwise, return 0.
 */
static int CheckColorSpace(const enum ColorSpace space)
{
    int index;

    // RGB is default
    if (space == HDMI_CS_RGB)
        return 1;

    // check EDID data is valid or not
    // read EDID
    if(!EDIDRead())
    {
        return 0;
    }

    // find VSDB
    for ( index = 1 ; index <= gExtensions ; index++ )
    {
        if (IsTimingExtension(index))  // if it's timing block
        {
            // read Color Space
            int CS = gEdidData[index*SIZEOFEDIDBLOCK + EDID_COLOR_SPACE_POS];

            if ( (space == HDMI_CS_YCBCR444 && (CS & EDID_YCBCR444_CS_MASK)) || // YCBCR444
                    (space == HDMI_CS_YCBCR422 && (CS & EDID_YCBCR422_CS_MASK)) ) // YCBCR422
            {
                return 1;
            }
        } // if
    } // for
    return 0;
}

/**
 * Check if EDID supports the colorimetry.
 * @param   color [in]    Colorimetry
 * @return  If EDID supports the colorimetry, return 1; Otherwise, return 0.
 */
static int CheckColorimetry(const enum HDMIColorimetry color)
{
    int index;

    // do not need to parse if not extended colorimetry
    if (color == HDMI_COLORIMETRY_NO_DATA || color == HDMI_COLORIMETRY_ITU601 || color == HDMI_COLORIMETRY_ITU709)
        return 1;

    // read EDID
    if(!EDIDRead())
    {
       return 0;
    }

    // find VSDB
    for ( index = 1 ; index <= gExtensions ; index++ )
    {
        if ( IsTimingExtension(index) ) // if it's timing block
        {
            // check address
            unsigned int ExtAddr = index*SIZEOFEDIDBLOCK + EDID_DATA_BLOCK_START_POS;
            unsigned int EndAddr = index*SIZEOFEDIDBLOCK + gEdidData[index*SIZEOFEDIDBLOCK + EDID_DETAILED_TIMING_OFFSET_POS];
            unsigned int tag,blockLen;

            // while
            while ( ExtAddr < EndAddr )
            {
                // find the block tag and length
                // tag
                tag = gEdidData[ExtAddr] & EDID_TAG_CODE_MASK;
                // block len
                blockLen = (gEdidData[ExtAddr] & EDID_DATA_BLOCK_SIZE_MASK) + 1;

                // check if it is colorimetry block
                if (tag == EDID_EXTENDED_TAG_VAL && // extended tag
                    gEdidData[ExtAddr+1] == EDID_EXTENDED_COLORIMETRY_VAL && // colorimetry block
                    (blockLen-1) == EDID_EXTENDED_COLORIMETRY_BLOCK_LEN )// check length
                {
                    // get supported DC value
                    int colorimetry = (gEdidData[ExtAddr + 2]);
                    int metadata = (gEdidData[ExtAddr + 3]);

                    DPRINTF("EDID extened colorimetry = %x\n",colorimetry);
                    DPRINTF("EDID gamut metadata profile = %x\n",metadata);

                    // check colorDepth
                    switch (color)
                    {
                        case HDMI_COLORIMETRY_EXTENDED_xvYCC601:
                            if (colorimetry & EDID_XVYCC601_MASK && metadata)
                                return 1;
                            break;
                        case HDMI_COLORIMETRY_EXTENDED_xvYCC709:
                            if (colorimetry & EDID_XVYCC709_MASK && metadata)
                                return 1;
                            break;
                        default:
                            break;
                    }
                    return 0;
                } // if VSDB block
                // else find next block
                ExtAddr += blockLen;
            } // while()
        } // if
    } // for

    return 0;
}

/**
 * Check if EDID supports the HDMI mode.
 * @return  If EDID supports HDMI mode, return 1; Otherwise, return 0.
 */
static int CheckHDMIMode(void)
{
    int index;

    // read EDID
    if(!EDIDRead())
    {
        return 0;
    }

    // find VSDB
    for ( index = 1 ; index <= gExtensions ; index++ )
    {
        if ( IsTimingExtension(index) ) // if it's timing extension block
        {
            if (GetVSDBOffset(index) > 0) // if there is a VSDB, it means RX support HDMI mode
                return 1;
        }
    }
    return 0;
}

/**
 * Initialize EDID library. This will intialize DDC library.
 * @return  If success, return 1; Otherwise, return 0.
 */
int EDIDOpen()
{
    // init DDC
    return DDCOpen();
}

/**
 * Finalize EDID library. This will finalize DDC library.
 * @return  If success, return 1; Otherwise, return 0.
 */
int EDIDClose()
{
    // reset EDID
    EDIDReset();

    // close EDDC
    return DDCClose();
}

/**
 * Read EDID data of Rx.
 * @return If success, return 1; Otherwise, return 0;
 */
int EDIDRead()
{
    int block,dataPtr;
    unsigned char temp[SIZEOFEDIDBLOCK];

    // if already read??
    if (EDIDValid())
        return 1;

    // read EDID Extension Number
    // read EDID
    if (!ReadEDIDBlock(0,temp))
    {
        return 0;
    }
    // get extension
    gExtensions = temp[EDID_EXTENSION_NUMBER_POS];

    // prepare buffer
    gEdidData = (unsigned char*)malloc((gExtensions+1)*SIZEOFEDIDBLOCK);
    if (!gEdidData)
        return 0;

    // copy EDID Block 0
    memcpy(gEdidData,temp,SIZEOFEDIDBLOCK);

    // read EDID Extension
    for ( block = 1,dataPtr = SIZEOFEDIDBLOCK; block <= gExtensions; block++,dataPtr+=SIZEOFEDIDBLOCK )
    {
        // read extension 1~gExtensions
        if (!ReadEDIDBlock(block, gEdidData+dataPtr))
        {
            // reset buffer
            EDIDReset();
            return 0;
        }
    }

    // check if extension is more than 1, and first extension block is not block map.
    if (gExtensions > 1 && gEdidData[SIZEOFEDIDBLOCK] != EDID_BLOCK_MAP_EXT_TAG_VAL)
    {
        // reset buffer
        DPRINTF("EDID has more than 1 extension but, first extension block is not block map\n");
        EDIDReset();
        return 0;
    }

    return 1;
}

/**
 * Reset stored EDID data.
 */
void EDIDReset()
{
    if (gEdidData)
    {
        free(gEdidData);
        gEdidData = NULL;
        DPRINTF("                                   EDID is reset!!!\n");
    }
}

/**
 * Get CEC physical address.
 * @param   outAddr [out]   CEC physical address. LSB 2 bytes is available. [0:0:AB:CD]
 * @return  If success, return 1; Otherwise, return 0.
 */
int EDIDGetCECPhysicalAddress(int* const outAddr)
{
    int index;
    unsigned int StartAddr;

    // check EDID data is valid or not
    // read EDID
    if(!EDIDRead())
    {
        return 0;
    }

    // find VSDB
    for ( index = 1 ; index <= gExtensions ; index++ )
    {
        if ( IsTimingExtension(index) // if it's timing block
            && (StartAddr = GetVSDBOffset(index)) > 0 )   // check block
        {
            // get supported DC value
            // int tempDC1 = (int)(gEdidData[tempAddr+EDID_DC_POS]);
            int phyAddr = gEdidData[StartAddr + EDID_CEC_PHYICAL_ADDR] << 8;
            phyAddr |= gEdidData[StartAddr + EDID_CEC_PHYICAL_ADDR+1];

            DPRINTF("phyAddr = %x\n",phyAddr);

            *outAddr = phyAddr;

            return 1;
        } // if
    } // for

    return 0;
}

/**
 * Check if Rx supports HDMI/DVI mode or not.
 * @param   video [in]   HDMI or DVI mode to check
 * @return  If Rx supports requested mode, return 1; Otherwise, return 0.
 */
int EDIDHDMIModeSupport(struct HDMIVideoParameter * const video)
{
    // check if read edid?
    if (!EDIDRead())
    {
        DPRINTF("EDID Read Fail!!!\n");
        return 0;
    }

    // check hdmi mode
    if (video->mode == HDMI)
    {
        if ( !CheckHDMIMode() )
        {
            DPRINTF("HDMI mode Not Supported\n");
            return 0;
        }
    }
    return 1;
}

/**
 * Check if Rx supports requested video resoultion or not.
 * @param   video [in]   Video parameters to check
 * @return  If Rx supports video parameters, return 1; Otherwise, return 0.
 */
int EDIDVideoResolutionSupport(struct HDMIVideoParameter * const video)
{
    // check if read edid?
    if (!EDIDRead())
    {
        DPRINTF("EDID Read Fail!!!\n");
        return 0;
    }

    // check resolution
    if ( !CheckResolution(video->resolution,video->pixelAspectRatio))
    {
        DPRINTF("Video Resolution Not Supported\n");
        return 0;
    }

    return 1;
}

/**
 * Check if Rx supports requested color depth or not.
 * @param   video [in]   Video parameters to check
 * @return  If Rx supports video parameters, return 1; Otherwise, return 0.
 */
int EDIDColorDepthSupport(struct HDMIVideoParameter * const video)
{
    // check if read edid?
    if (!EDIDRead())
    {
        DPRINTF("EDID Read Fail!!!\n");
        return 0;
    }

    // check resolution
    if ( !CheckColorDepth(video->colorDepth,video->colorSpace) )
    {
        DPRINTF("Color Depth Not Supported\n");
        return 0;
    }

    return 1;
}

/**
 * Check if Rx supports requested color space or not.
 * @param   video [in]   Video parameters to check
 * @return  If Rx supports video parameters, return 1; Otherwise, return 0.
 */
int EDIDColorSpaceSupport(struct HDMIVideoParameter * const video)
{
    // check if read edid?
    if (!EDIDRead())
    {
        DPRINTF("EDID Read Fail!!!\n");
        return 0;
    }
    // check color space
    if ( !CheckColorSpace(video->colorSpace) )
    {
        DPRINTF("Color Space Not Supported\n");
        return 0;
    }

    return 1;
}

/**
 * Check if Rx supports requested colorimetry or not.
 * @param   video [in]   Video parameters to check
 * @return  If Rx supports video parameters, return 1; Otherwise, return 0.
 */
int EDIDColorimetrySupport(struct HDMIVideoParameter * const video)
{
    // check if read edid?
    if (!EDIDRead())
    {
        DPRINTF("EDID Read Fail!!!\n");
        return 0;
    }

    // check colorimetry
    if ( !CheckColorimetry(video->colorimetry) )
    {
        DPRINTF("Colorimetry Not Supported\n");
        return 0;
    }

    return 1;
}


/**
 * Check if Rx supports requested audio parameters or not.
 * @param   audio [in]   Audio parameters to check
 * @return  If Rx supports audio parameters, return 1; Otherwise, return 0.
 */
int EDIDAudioModeSupport(struct HDMIAudioParameter * const audio)
{
    int index;

    // read EDID
    if(!EDIDRead())
    {
        DPRINTF("EDID Read Fail!!!\n");
        return 0;
    }

    // check EDID Extension

    // find timing block
    for ( index = 1 ; index <= gExtensions ; index++ )
    {
        if ( IsTimingExtension(index) ) // if it's timing block
        {
            // find Short Audio Description
            unsigned int StartAddr = index*SIZEOFEDIDBLOCK;
            unsigned int ExtAddr = StartAddr + EDID_DATA_BLOCK_START_POS;
            unsigned int tag,blockLen;
            unsigned int DTDStartAddr = gEdidData[StartAddr + EDID_DETAILED_TIMING_OFFSET_POS];

            // while
            while ( ExtAddr < StartAddr + DTDStartAddr )
            {
                // find the block tag and length
                // tag
                tag = gEdidData[ExtAddr] & EDID_TAG_CODE_MASK;
                // block len
                blockLen = (gEdidData[ExtAddr] & EDID_DATA_BLOCK_SIZE_MASK) + 1;

                DPRINTF("tag = %d\n",tag);
                DPRINTF("blockLen = %d\n",blockLen-1);

                // check if it is short video description
                if (tag == EDID_SHORT_AUD_DEC_TAG_VAL)
                {
                    // if so, check SAD
                    int i;
                    int audioFormat,channelNum,sampleFreq,wordLen;
                    for (i=1; i < blockLen; i+=3)
                    {
                        audioFormat = gEdidData[ExtAddr+i] & EDID_SAD_CODE_MASK;
                        channelNum = gEdidData[ExtAddr+i] & EDID_SAD_CHANNEL_MASK;
                        sampleFreq = gEdidData[ExtAddr+i+1];
                        wordLen = gEdidData[ExtAddr+i+2];

                        DPRINTF("request = %d, EDIDAudioFormatCode = %d\n",(audio->formatCode)<<3, audioFormat);
                        DPRINTF("request = %d, EDIDChannelNumber= %d\n",(audio->channelNum)-1, channelNum);
                        DPRINTF("request = %d, EDIDSampleFreq= %d\n",1<<(audio->sampleFreq), sampleFreq);
                        DPRINTF("request = %d, EDIDWordLeng= %d\n",1<<(audio->wordLength), wordLen);

                        // check parameter
                        // check audioFormat
                        if ( audioFormat == ( (audio->formatCode) << 3)     &&  // format code
                                channelNum >= ( (audio->channelNum) -1)         &&  // channel number
                                ( sampleFreq & (1<<(audio->sampleFreq)) )      ) // sample frequency
                        {
                            if (audioFormat == LPCM_FORMAT) // check wordLen
                            {
                                int ret = 0;
                                switch ( audio->wordLength )
                                {
                                    case WORD_16:
                                    case WORD_17:
                                    case WORD_18:
                                    case WORD_19:
                                    case WORD_20:
                                        ret = wordLen & (1<<1);
                                        break;
                                    case WORD_21:
                                    case WORD_22:
                                    case WORD_23:
                                    case WORD_24:
                                        ret = wordLen & (1<<2);
                                        break;
                                }
                                return ret;
                            }
                            return 1; // if not LPCM
                        }
                    } // for
                } // if tag
                // else find next block
                ExtAddr += blockLen;
            } // while()
        } // if
    } // for

    return 0;
}
