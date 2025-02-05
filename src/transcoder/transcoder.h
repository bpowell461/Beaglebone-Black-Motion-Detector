#ifndef TRANSCODER_H_
#define TRANSCODER_H_

void transcoder_init(int *fd);
void *transcoder_task(void *threadp);

#endif // !TRANSCODER_H_

