#ifndef UI_DISPLAY_H
#define UI_DISPLAY_H

void ui_display_init(void);
void ui_display_update_imu(int player, float gy, float gz);
void ui_display_update_audio(const char *event);
void ui_display_update_score(int p1_score, int p2_score, int high_score);
void ui_display_update_gesture(const char *gesture);
void ui_display_tick(void);

#endif