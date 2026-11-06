/* The rules of the chords, apart from the hardware, so they are tested on the host. */
#ifndef AMIGA_SNDLOGIC_H
#define AMIGA_SNDLOGIC_H

#define SND_NONE 0xFF

/* per chord (platform.h order), up to three notes of {Hz, milliseconds}; Hz 0 ends the chord */
extern const unsigned short amiga_chord_notes[7][3][2];

struct snd_state {
    unsigned char on;                   /* the player's switch */
    unsigned char playing;              /* the chord last started, or SND_NONE */
    unsigned char end;                  /* the tick at which it is over */
};

/* A chord still sounding is cut off only by a higher-numbered one. */
unsigned char amiga_chord_wins(unsigned char playing, unsigned char incoming);

/* The chord to start now (and the state to match), or SND_NONE to stay as it is. `now` is the 8-bit tick. */
unsigned char amiga_snd_request(struct snd_state *s, unsigned char chord, unsigned char now, unsigned char tps);
unsigned char amiga_snd_toggle(struct snd_state *s, unsigned char now, unsigned char tps);

/* Paula's period for one sample byte per 1/8 cycle of `hz` on a clock of `clock` Hz, and the cycles that last `ms`. */
unsigned short amiga_note_period(unsigned short hz, unsigned long clock);
unsigned short amiga_note_cycles(unsigned short hz, unsigned short ms);

#endif
