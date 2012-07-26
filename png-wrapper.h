#ifndef PNG_WRAPPER_H_
#define PNG_WRAPPER_H_

#ifdef __cplusplus
extern "C" {
#endif

int
png_write (const char *file_name, unsigned int width, unsigned int height, unsigned char* data);

int
png_load (const char* path, void **ret_data, unsigned int *ret_width, unsigned int *ret_height);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* !PNG_H_ */
