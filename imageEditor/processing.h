#ifndef PROCESSING_H
#define PROCESSING_H

#include "image.h"

/* --- Operations that modify the image IN PLACE --- */

/* Convert to grayscale using gray = 0.299R + 0.587G + 0.114B */
void apply_grayscale(Image *img);

/* Add 'delta' to every R,G,B component, clamped to [0,255].
   delta may be negative (to darken) or positive (to brighten). */
void apply_brightness(Image *img, int delta);

/* Negative-like inversion: R=255-R, G=255-G, B=255-B */
void apply_invert(Image *img);

/* Mirror left-right */
void apply_flip_horizontal(Image *img);

/* Mirror top-bottom */
void apply_flip_vertical(Image *img);


/* --- Operations that produce a NEW image (caller must image_free it) --- */

/* Rotate 90 degrees clockwise. Result has width/height swapped. */
Image* apply_rotate90(const Image *img);

/* Extract the rectangular region starting at (x,y) with size (w,h).
   Caller is responsible for ensuring the region fits inside the image. */
Image* apply_crop(const Image *img, int x, int y, int w, int h);

/* 3x3 box blur (average of the pixel and its 8 neighbors) */
Image* apply_blur(const Image *img);

/* Optional bonus feature: 3x3 sharpening convolution */
Image* apply_sharpen(const Image *img);

#endif /* PROCESSING_H */
