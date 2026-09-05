#ifndef IMAGE_H
#define IMAGE_H

/* A single pixel: red, green, blue components (0-255 each) */
typedef struct {
    unsigned char r;
    unsigned char g;
    unsigned char b;
} Pixel;

/* A full image: width, height, and a dynamically allocated pixel array.
   Pixel (x, y) is stored at data[y * width + x].
   Row 0 is the TOP row of the image. */
typedef struct {
    int width;
    int height;
    Pixel *data;
} Image;

/* Allocate a new image of the given size (pixel data is uninitialized).
   Returns NULL on allocation failure. */
Image* image_create(int width, int height);

/* Free an image and its pixel data. Safe to call with NULL. */
void image_free(Image *img);

/* Create a full deep copy of an image (used for Undo). */
Image* image_copy(const Image *img);

/* Load a 24-bit uncompressed BMP file into memory.
   Returns NULL if the file cannot be opened or is not a supported BMP. */
Image* image_load_bmp(const char *filename);

/* Save an image as a 24-bit uncompressed BMP file.
   Returns 1 on success, 0 on failure. */
int image_save_bmp(const Image *img, const char *filename);

#endif /* IMAGE_H */
