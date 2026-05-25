#ifndef AUDIO_OUT_H
#define AUDIO_OUT_H

void audio_out_init(void);
void audio_out_play_event(const char *event);
void audio_out_queue_event(const char *event);

#endif