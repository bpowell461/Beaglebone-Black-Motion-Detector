#ifndef FRAME_BUFFER_H_
#define FRAME_BUFFER_H_

#include "types.h"
#include "camera_cfg.h"

sys_result_e framebuffer_init(INT32 *fd);
sys_result_e framebuffer_initframebuffers(INT32 fd);
sys_result_e framebuffer_getframe_ptr(INT32 fd, frame_t **frame);
sys_result_e framebuffer_freeframe(INT32 fd, frame_t *frame);
sys_result_e framebuffer_writeframe(INT32 fd);
sys_result_e framebuffer_getframe(INT32 fd, frame_t *frame);
sys_result_e framebuffer_deinit(void);

#endif
