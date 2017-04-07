#include "trim.h"

/*
In a Unix terminal application, there seem to be two main ways to read input in a manner suitable for non-printing.

The simple way is to read raw data from stdin after configuring the TTY to turn off unwanted features.
The downside to this approach is that there seems to be no good way of determining if a key has been released,
which is essential for determining whether a key is held or not, which a lot of games rely on, especially for movement.
This approach should work on all Unix-based systems, however.

The second approach is by reading data from the 'evdev' interface, which is currently only supported by Linux and the latest FreeBSD builds.
It does, however, allow for accurately deciding if a key is held or not by the event driven nature of the interface.

In a Windows command-line application, 
*/

void TRIM_InitKB(int kb_mode) {
	if (TRIM_input) return;
	TRIM_kb_mode = kb_mode;

	TRIM_old_kbst = NULL;
	TRIM_cur_kbst = NULL;
	TRIM_old_kbsize = 0;
	TRIM_cur_kbsize = 0;

#ifdef _WIN32_
	TRIM_winput = GetStdHandle(STD_INPUT_HANDLE);
	int kc[] = {
		0xc0, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30, 0xbd, 0xbb, 0x08,
		0x09, 0x51, 0x57, 0x45, 0x52, 0x54, 0x59, 0x55, 0x49, 0x4f, 0x50, 0xdb, 0xdd, 0xdc,
		0x14, 0x41, 0x53, 0x44, 0x46, 0x47, 0x48, 0x4a, 0x4b, 0x4c, 0xba, 0xde, 0x0d,
		0xa0, 0x5a, 0x58, 0x43, 0x56, 0x42, 0x4e, 0x4d, 0xbc, 0xbe, 0xbf, 0xa1,
		0xa2, 0x5b, 0xa4, 0x20, 0xa5, 0x5c, 0x5d, 0xa3,
		0x1b, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x7b,
		0x2c, 0x91, 0x13, 0x2d, 0x24, 0x21, 0x2e, 0x23, 0x22, 0x26, 0x25, 0x28, 0x27,
		0x90, 0x6f, 0x6a, 0x6d, 0x67, 0x68, 0x69, 0x6b, 0x64, 0x65, 0x66, 0x61, 0x62, 0x63, 0x0d, 0x60, 0x6e
	};
	memcpy((int*)TRIM_keycode, &kc[0], sizeof(kc));
#else
	TRIM_kbfd = -1;

	char path[32];
	unsigned char bits[32];
	int idx = -1, i;
	struct stat st;

	// stdin & TTY configuration code taken from gcat.co.uk
	struct termios tty;

	/* make stdin non-blocking */
	int flags = fcntl(0, F_GETFL);
	flags |= O_NONBLOCK;
	fcntl(0, F_SETFL, flags);

	/* turn off buffering, echo and key processing */
	tcgetattr(0, &_tty_old);
	tty = _tty_old;
	tty.c_lflag &= ~(ICANON | ECHO | ISIG);
	tty.c_iflag &= ~(ISTRIP | INLCR | ICRNL | IGNCR | IXON | IXOFF);
	tcsetattr(0, TCSANOW, &tty);

	FD_ZERO(&_TRIM_fdset);
	FD_SET(0, &_TRIM_fdset);

	if (kb_mode == TRIM_DEFKB) {
		int kc[] = {
			0x60, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30, 0x2d, 0x3d, 0x7f,
			0x09, 0x71, 0x77, 0x65, 0x72, 0x74, 0x79, 0x75, 0x69, 0x6f, 0x70, 0x5b, 0x5d, 0x5c,
			0xff, 0x61, 0x73, 0x64, 0x66, 0x67, 0x68, 0x6a, 0x6b, 0x6c, 0x3b, 0x27, 0x0d,
			0xff, 0x7a, 0x78, 0x63, 0x76, 0x62, 0x6e, 0x6d, 0x2c, 0x2e, 0x2f, 0xff,
			0xff, 0xff, 0xff, 0x20, 0xff, 0xff, 0xff, 0xff,
			0x1b, 0x1b4f50, 0x1b4f51, 0x1b4f52, 0x1b4f53, 0x5b31357e, 0x5b31377e, 0x5b31387e, 0x5b31397e, 0x5b32307e, 0x5b32317e, 0x5b32337e, 0x5b32347e,
			0xff, 0xff, 0xff, 0x1b5b327e, 0x1b5b48, 0x1b5b357e, 0x1b5b337e, 0x1b5b46, 0x1b5b367e, 0x1b5b41, 0x1b5b44, 0x1b5b42, 0x1b5b43,
			0xff, 0x2f, 0x2a, 0x2d, 0x37, 0x38, 0x39, 0x2b, 0x34, 0x35, 0x36, 0x31, 0x32, 0x33, 0x0d, 0x30, 0x2e
		};
		memcpy((int*)TRIM_keycode, &kc[0], sizeof(kc));
		
		TRIM_kbfd = 0;
		TRIM_input = 1;
		return;
	}

	// Attempt opening an input event device
	while (1) {
		idx++;
		sprintf(path, "/dev/input/event%d", idx);
		if (stat(path, &st) < 0) {
			TRIM_CloseKB();
			return;
		}

		TRIM_kbfd = open(path, O_RDONLY);
		if (TRIM_kbfd < 0) continue;

		memset(bits, 0, 32);
		if (ioctl(TRIM_kbfd, EVIOCGBIT(0, sizeof(bits)), bits) < 0) {
			close(TRIM_kbfd);
			continue;
		}

		// Check if the first three bytes are 0x13, 0x00 and 0x12 respectively
		// If not, try another input device
		for (i = 0; i < 3; i++)
			if (bits[i] != "C0B"[i]-'0') break;

		if (i == 3) {
			_TRIM_kb_mode = TRIM_RAWKB;
			FD_ZERO(&_TRIM_fdset);
			FD_SET(TRIM_kbfd, &_TRIM_fdset);
			break;
		}
		close(TRIM_kbfd);
	}

	int kc[] = {
		0x29, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
		0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x2b,
		0x3a, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x1c,
		0x2a, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36,
		0x1d, 0x7d, 0x38, 0x39, 0x64, 0x7d, 0x7f, 0x61,
		0x01, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43, 0x44, 0x57, 0x58,
		0x63, 0x46, 0x77, 0x6e, 0x66, 0x68, 0x6f, 0x6b, 0x6d, 0x67, 0x69, 0x6c, 0x6a,
		0x45, 0x62, 0x37, 0x4a, 0x47, 0x48, 0x49, 0x4e, 0x4b, 0x4c, 0x4d, 0x4f, 0x50, 0x51, 0x60, 0x52, 0x53
	};
	memcpy((int*)TRIM_keycode, &kc[0], sizeof(kc));
#endif /* #ifndef _WIN32_ */

	TRIM_input = 1;
}

void TRIM_ReadInput(int *key, int wait) {
	if (!TRIM_input) {
		if (key) *key = 0;
		return;
	}

	if (TRIM_cur_kbst && TRIM_cur_kbsize > 0) {
		TRIM_old_kbst = realloc(TRIM_old_kbst, TRIM_cur_kbsize * sizeof(TRIM_Key));
		TRIM_old_kbsize = TRIM_cur_kbsize;
		memcpy(TRIM_old_kbst, TRIM_cur_kbst, TRIM_old_kbsize);
	}

#ifdef _WIN32_

#else
	if (TRIM_kb_mode == TRIM_DEFKB) {
		int i;
		for (i = 0; i < TRIM_cur_kbsize; i++) {
			TRIM_cur_kbst[i].state = 0;
			if (i < TRIM_old_kbsize) TRIM_old_kbst[i].state = 0;
		}

		u8 buf[16] = {0};
		if (wait) select(1, &_TRIM_fdset, NULL, NULL, NULL);
		read(0, &buf[0], 16);

		int value = 0, l = 0;
		for (i = sz-1; i >= 0 && l < 4; i--) {
			if (buf[i] || l) {
				value |= (int)buf[i] << (l * 8);
				l++;
			}
		}

		for (i = 0; i < TRIM_cur_kbsize; i++) {
			if (TRIM_cur_kbst[i].code == value) {
				TRIM_cur_kbst[i].state = 1;
				break;
			}
		}
		if (i == TRIM_cur_kbsize) {
			TRIM_cur_kbst = realloc(TRIM_cur_kbst, ++TRIM_cur_kbsize * sizeof(TRIM_Key));
			TRIM_cur_kbst[i].code = value;
			TRIM_cur_kbst[i].state = 1;
		}
		if (key) *key = i;
	}
	else {
  #ifdef __linux__
		struct input_event ev[64];

		if (wait) select(1, &_TRIM_fdset, NULL, NULL, NULL);
		int r = read(TRIM_kbfd, ev, sizeof(ev));
		if (r < 0) {
			printf("Could not read from evdev (fd: %d, r: %d)\n", TRIM_kbfd, r);
			return;
		}

		int i, j, n_events = r / sizeof(struct input_event);
		for (i = 0; i < n_events; i++) {
			if (ev[i].type > 1) continue;
			for (j = 0; j < TRIM_cur_kbsize; j++) {
				if (TRIM_cur_kbst[j].code == ev[i].code) {
					TRIM_cur_kbst[j].state = ev[i].value;
					break;
				}
			}
			if (j == TRIM_cur_kbsize) {
				TRIM_cur_kbst = realloc(TRIM_cur_kbst, ++TRIM_cur_kbsize * sizeof(TRIM_Key));
				TRIM_cur_kbst[j].code = ev[i].code;
				TRIM_cur_kbst[j].state = ev[i].value;
			}
		}
		if (key) *key = j;
  #endif /* ifdef __linux__ */
	}
#endif /* ifndef _WIN32_ */
}

// TODO: Flush evdev fd
void TRIM_CloseKB(void) {
	if (!TRIM_input) return;

	if (TRIM_old_kbst) free(TRIM_old_kbst);
	if (TRIM_cur_kbst) free(TRIM_cur_kbst);

	int flags = fcntl(0, F_GETFL);
	flags &= ~O_NONBLOCK;
	fcntl(0, F_SETFL, flags);
	tcsetattr(0, TCSANOW, &_tty_old);

	if (TRIM_kbfd > 0) close(TRIM_kbfd);

	TRIM_input = 0;
}

#endif /* ifndef _WIN32_ */

void TRIM_PollKB(void) {
	TRIM_ReadInput(NULL, 0);
}

int TRIM_GetKey(void) {
	if (!TRIM_input) return;

	int key = 0;
	TRIM_ReadInput(&key, 1);
	if (TRIM_cur_kbst && key < TRIM_cur_kbsize) return TRIM_cur_kbst[key].code;
	return key;
}

int TRIM_KeyDown(int key) {
	if (!TRIM_input) return 0;

	int i;
	for (i = 0; i < TRIM_cur_kbsize; i++) {
		if (TRIM_cur_kbst[i].code == key && (i >= TRIM_old_kbsize ||
		    (TRIM_old_kbst[i].state == 0 && TRIM_cur_kbst[i].state == 1))) {
			return 1;
		}
	}
	return 0;
}

int TRIM_KeyHeld(int key) {
	if (!TRIM_input) return 0;

	int i;
	for (i = 0; i < TRIM_cur_kbsize; i++) {
		if (TRIM_cur_kbst[i].code == key && TRIM_cur_kbst[i].state == 1) {
			return 1;
		}
	}
	return 0;
}

int TRIM_KeyUp(int key) {
	if (!TRIM_input) return 0;

	int i;
	for (i = 0; i < TRIM_old_kbsize; i++) {
		if (TRIM_cur_kbst[i].code == key &&
		    TRIM_old_kbst[i].state == 1 &&
		    TRIM_cur_kbst[i].state == 0) {
			return 1;
		}
	}
	return 0;
}
