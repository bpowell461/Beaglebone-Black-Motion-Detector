#ifndef FRAME_BUFFER_H_
#define FRAME_BUFFER_H_

#include "types.h"

#define SAVED_FRAMES_MAX    (5U)

sys_result_e framebuffer_init(INT32 *fd);
sys_result_e framebuffer_writeframe(INT32 fd);
sys_result_e framebuffer_getframe(INT32 fd);
UINT16       framebuffer_getframeidx(void);
sys_result_e framebuffer_deinit(void);

#endif
