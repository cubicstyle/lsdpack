#include <cstdio>
#include <cstdlib>

#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#include "gambatte.h"

#include "input.h"
#include "writer.h"

int written_songs;
gambatte::GB gameboy;
Input input;
std::string out_path;

double start_time, timeout;

void run_one_frame() {
    size_t samples = 35112;
    long unsigned int audioBuffer[35112 + 2064];
    gameboy.runFor(0, 0, &audioBuffer[0], samples);
}

void wait(float seconds) {
    for (float i = 0.f; i < 60.f * seconds; ++i) {
        run_one_frame();
    }
}

void press(unsigned key, float seconds = 0.1f) {
    input.press(key);
    wait(seconds);
}

void load_song(int position) {
    press(SELECT);
    press(SELECT | UP);
    press(0);
    press(DOWN, 3);
    press(0);
    press(A);
    press(0);
    press(A);
    press(0);
    press(UP, 5); // scroll to top
    press(0);
    for (int i = 0; i < position; ++i) {
        press(DOWN);
        press(0);
    }
    // press song name
    press(A);
    press(0);
    // discard changes
    press(LEFT);
    press(0);
    press(A);
    // wait until song is loaded
    press(0, 5);
    if (gameboy.isSongEmpty()) {
        write_music_to_disk();
        puts("OK");
        exit(0);
    }
    printf("Song %i...\n", ++written_songs);
}

bool sound_enabled;

void play_song() {
    sound_enabled = false;
    input.press(START);
    record_song_start(out_path.c_str());
    do {
        wait(1);

        if( (clock() / CLOCKS_PER_SEC) - start_time > timeout){
            fprintf(stderr, "record timeout!\n");
            exit(2);
        }

    } while(sound_enabled);
}

void on_ff_write(char p, char data) {
    if (p < 0x10 || p >= 0x40) {
        return; // not sound
    }
    switch (p) {
        case 0x26:
            if (sound_enabled && !data) {
                record_song_stop();
                sound_enabled = false;
                return;
            }
            sound_enabled = data;
            break;
    }
    if (sound_enabled) {
        record_write(p, data);
    }
}

void on_lcd_interrupt() {
    if (sound_enabled) {
        record_lcd();
    }
}

void make_out_path(const char* in_path) {
    out_path = in_path;
    // .gb => .s
    out_path.replace(out_path.end() - 2, out_path.end(), "s");
    out_path.replace(out_path.begin(), out_path.begin() + out_path.rfind('/') + 1, "");
    out_path.replace(out_path.begin(), out_path.begin() + out_path.rfind('\\') + 1, "");
    printf("Recording to '%s'\n", out_path.c_str());
}

#define DEF_TIMEOUT 60.0
#define DEF_MBC '5'
#define DEF_MAXBANK 0x200
#define DEF_STARTBANK 1

void usage(){
    fprintf(stderr, "usage: lsdpack <lsdj.gb>\n");
    fprintf(stderr, "[-t sec]:\t\t\ttimeout sec 0-120 (default %d sec)\n", (int)DEF_TIMEOUT);
    fprintf(stderr, "[-s dir]:\t\t\tSavedata dir\n");
    fprintf(stderr, "[-d dir]:\t\t\tOutput dir\n");
    // fprintf(stderr, "[-B mbc]:\t\t\tUse MBC \"C\",\"5\" (default:MBC%d)\n",DEF_MBC);
    fprintf(stderr, "[-M maxbank]:\t\t\tMax bank num (default:1023)\n");
    fprintf(stderr, "[-S starting bank]:\t\tStarting bank num (default:1)\n");
    fprintf(stderr, "[-h]:\t\t\t\tHelp\n");
}

int main(int argc, char* argv[]) {
    int op, i, t;
    int startbank = DEF_STARTBANK;
    int maxbank = DEF_MAXBANK;
	//char mbc = DEF_MBC;
    char destpath[256];
    struct stat statinfo;

    memset(destpath, 0, 256);
    timeout = DEF_TIMEOUT;

    if (argc < 2) {
        usage();
        return 1;
    }

    opterr = 0;
    while ((op = getopt(argc, argv, "ht:s:d:B:M:S:")) != -1) {
        switch (op) {
            case 'h':
                usage();
                return 1;
            case 't':
				t = atoi(optarg);
		        if(t <= 0 || t > 120){
		        	fprintf(stderr, "usage: timeout sec error\n");
		        	return 1;
		        }
		        timeout = (double) t;
                break;
            case 's':
		        if (stat(optarg, &statinfo) == 0){
		            gameboy.setSaveDir(optarg);
		        }
		        else{
		            fprintf(stderr, "usage: savedata dir error\n");
		            return 1;
		        }
                break;
            case 'd':
                strncpy(destpath, optarg, 255);
                /*
                if (stat(optarg, &statinfo) == 0){
		            strncpy(destpath, optarg, 255);
		        }
		        else{
		            fprintf(stderr, "usage: dest dir error\n");
		            return 1;
		        }
		        */
                break;
                /*
            case 'B':
                if(  strcmp("C",optarg) != 0 && strcmp("5",optarg) != 0 ){
		            fprintf(stderr, "usage: MBC error\n");
		            return 1;
                }
                mbc = optarg[0];
                break;
                */
            case 'M':
				maxbank = atoi(optarg);
		        if(maxbank < 32 || maxbank > 0x200){
		        	fprintf(stderr, "usage: Max bank num error\n");
		        	return 1;
		        }
                break;
            case 'S':
				startbank = atoi(optarg);
		        if(startbank < 1 || startbank > 0x200){
		        	fprintf(stderr, "usage: Starting bank num error\n");
		        	return 1;
		        }
                break;
            default:
                usage();
                return 1;
        }
    }

	if(startbank >= maxbank){
		fprintf(stderr, "Starting bank num exceeds maximum bank num error\n");
		return 1;
	}
    
    gameboy.setInputGetter(&input);
    gameboy.setWriteHandler(on_ff_write);
    gameboy.setLcdHandler(on_lcd_interrupt);
    gameboy.load(argv[optind]);

    if(strlen(destpath) != 0)
        out_path = destpath;
    else
        make_out_path(argv[optind]);

    press(0, 3);

	set_startbank(startbank);
	set_maxbank(maxbank);
	
    // start timeout timer
    start_time = clock() / CLOCKS_PER_SEC ;

    i = 0;
    while (true) {
        load_song(i++);
        play_song();
    }
}
