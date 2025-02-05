#ifndef FRAME_BUFFER_H_
#define FRAME_BUFFER_H_

#include "types.h"
#include "camera_cfg.h"

sys_result_e framebuffer_init(int *fd);
sys_result_e framebuffer_initframebuffers(int fd);
sys_result_e framebuffer_getframe_ptr(int fd, frame_t **frame);
sys_result_e framebuffer_freeframe(int fd, frame_t *frame);
sys_result_e framebuffer_writeframe(int fd, uint8_t saveFrame);
sys_result_e framebuffer_getframe(int fd, frame_t *frame);
sys_result_e framebuffer_deinit(void);

#endif
