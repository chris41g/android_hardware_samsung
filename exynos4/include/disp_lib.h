/*
 * Copyright@ Samsung Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#ifndef __SAMSUNG_SYSLSI_DISPLIB_H__
#define __SAMSUNG_SYSLSI_DISPLIB_H__

#ifdef __cplusplus
extern "C" {
#endif
int open_direct_lcd(int frame_width, int frame_height, int fd_pmem);
int close_direct_lcd(void);
unsigned int open_fimc_for_hdmi(int frame_width, int frame_height, int fd_pmem, unsigned int offset, int *hdmi_width, int *hdmi_height);
int close_fimc_for_hdmi(void);
void set_video_buffer(unsigned int offset);
#ifdef __cplusplus
}
#endif

#endif //__SAMSUNG_SYSLSI_APDEV_HDMILIB_H__
