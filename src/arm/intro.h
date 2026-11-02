/*
 * The intro: the camera pans up a tall tower at dusk to the hero standing on its
 * roof, then the title appears. The tower is generated from a few numbers (no
 * stored picture), so it costs almost no ROM.
 */
#ifndef INTRO_H
#define INTRO_H

/* Start the pan from the bottom of the tower. */
void intro_enter(void);

/* Draw one frame. tick counts frames since intro_enter(). */
void intro_frame(unsigned tick);

/* 1 once the pan is done and the title and prompt are showing. */
int intro_ready(void);

/* Jump straight to the end of the pan. */
void intro_skip(void);

#endif /* INTRO_H */
