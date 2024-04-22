#ifndef IMAGE_H_
#define IMAGE_H_

#include "types.h"
#include "camera_cfg.h"

sys_result_e imagebuffer_init(void);
sys_result_e imagebuffer_startread(rgb_frame_t *frame);
sys_result_e imagebuffer_endread(void);
sys_result_e imagebuffer_write(rgb_frame_t *frame);
sys_result_e image_convert(UINT32 srcFmt, UINT32 destFmt, const UINT08 *src_frame, UINT08 *dest_frame);
sys_result_e image_save(const UINT08 *buf, const UINT32 size);
UINT08       image_getsavedframes(void);

#endif // !IMAGE_H_
