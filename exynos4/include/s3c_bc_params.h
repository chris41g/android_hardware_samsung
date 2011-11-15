/*!****************************************************************************
@File           s3c_bc_params.h

@Title          s3c_bc kernel driver parameters

@Author         Imagination Technologies
		Samsung Electronics Co. LTD

@Date           03/03/2010

@Copyright      Licensed under the Apache License, Version 2.0 (the "License");
		you may not use this file except in compliance with the License.
		You may obtain a copy of the License at

		http://www.apache.org/licenses/LICENSE-2.0

		Unless required by applicable law or agreed to in writing, software
		distributed under the License is distributed on an "AS IS" BASIS,
		WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
		See the License for the specific language governing permissions and
		limitations under the License.

@Platform       Generic

@Description    s3c_bc kernel driver parameters

@DoxygenVer

******************************************************************************/

/******************************************************************************
Modifications :-
$Log: s3c_bc_params.h $
******************************************************************************/

#define S3C_BC_DEVICE_NAME		"S3C_BC_DEV"
#define S3C_BC_DEVICE_ID		0
#define S3C_BC_DEVICE_BUFFER_COUNT	2		/* TODO: Modify this accordingly. */
//#define S3C_BC_DEVICE_PHYS_ADDR_START	0x60000000	/* TODO: Modify this based on the memory reservation. */

#if defined SLSI_SMDKC110
#define S3C_BC_DEVICE_PHYS_ADDR_START	0x34a00000	/* TODO: Modify this based on the memory reservation. */
#endif
#if defined SLSI_SMDKV210
#define S3C_BC_DEVICE_PHYS_ADDR_START	0x3FA00000	/* TODO: Modify this based on the memory reservation. */
#endif

#define S3C_BC_DEVICE_WIDTH		800		/* Screen width. */
#define S3C_BC_DEVICE_HEIGHT		800		/* Screen height. */
#define S3C_BC_DEVICE_PIXELFORMAT	(PVRSRV_PIXEL_FORMAT_ARGB8888)
#define S3C_BC_DEVICE_STRIDE		(S3C_BC_DEVICE_WIDTH*4)
#define S3C_BC_DEVICE_PHYS_PAGE_SIZE	0x1000		/* 4KB */
#define S3C_BC_DEVICE_BUF_SIZE		((S3C_BC_DEVICE_HEIGHT * S3C_BC_DEVICE_STRIDE + S3C_BC_DEVICE_PHYS_PAGE_SIZE - 1) & ~(S3C_BC_DEVICE_PHYS_PAGE_SIZE-1))

/******************************************************************************
 End of file (s3c_bc_params.h)
******************************************************************************/
