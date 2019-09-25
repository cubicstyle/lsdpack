#pragma once

void record_song_start(const char* out_path);
void record_song_stop();
void record_write(unsigned char addr, unsigned char data);
void record_lcd();
void write_music_to_disk();
void set_startbank(int banknum);
void set_maxbank(int banknum);
