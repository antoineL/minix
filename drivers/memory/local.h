/*
local defines and declarations 
*/

extern unsigned char _binary_image_mfs_start[], *_binary_image_mfs_end;
extern size_t _binary_image_mfs_size;

#define	imgrd		_binary_image_mfs_start
#define	imgrd_size	_binary_image_mfs_size
#define	imgrd_sizeX \
	((size_t)(_binary_image_mfs_end - _binary_image_mfs_start))
