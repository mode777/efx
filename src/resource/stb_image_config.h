#ifndef EFX_STB_IMAGE_CONFIG_H
#define EFX_STB_IMAGE_CONFIG_H

/*
 * Shared stb_image configuration for the resource loader (F6a): PNG + JPEG
 * only, memory decode only (the provider owns file access). Included before
 * stb_image.h by both the implementation TU and the decode TU so their
 * declarations agree.
 */
#define STBI_NO_BMP
#define STBI_NO_GIF
#define STBI_NO_PSD
#define STBI_NO_TGA
#define STBI_NO_HDR
#define STBI_NO_PIC
#define STBI_NO_PNM
#define STBI_NO_STDIO

#endif
