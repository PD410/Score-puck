// Updated mets_logo.h with scaling support

#ifndef METS_LOGO_H
#define METS_LOGO_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>

// Logo dimensions
extern const int METS_LOGO_WIDTH;
extern const int METS_LOGO_HEIGHT;
extern const unsigned char mets_logo_bitmap[];

// Function to draw inverted logo (for solid backgrounds)
void drawMetsLogoInverted(int x, int y, uint16_t fgColor, uint16_t bgColor);

// Function to draw logo with gradient background preservation and scaling support
// Default scale = 1 (original size), scale = 2 (half size), scale = 3 (third size), etc.
void drawMetsLogoWithGradientBackground(int x, int y, uint16_t logoColor, int scale = 1);

// Helper function to get scaled logo dimensions
void getScaledLogoDimensions(int scale, int* width, int* height);

#endif