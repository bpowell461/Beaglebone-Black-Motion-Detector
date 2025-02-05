#ifndef IMAGE_H_
#define IMAGE_H_

#include "types.h"
#include "camera_cfg.h"

sys_result_e imagebuffer_init(void);
sys_result_e imagebuffer_startread(rgb_frame_t **frame);
sys_result_e imagebuffer_endread(void);
sys_result_e imagebuffer_write(rgb_frame_t *frame);
sys_result_e image_convert(uint32_t srcFmt, uint32_t destFmt, const uint8_t *src_frame, uint8_t *dest_frame);
sys_result_e image_save(const rgb_frame_t *buf, const uint32_t size);
uint32_t       image_getsavedframes(void);

#endif // !IMAGE_H_
