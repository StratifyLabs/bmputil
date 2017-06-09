#include <cstdio>

#include <iface/dev/display.h>
#include <stfy/sys.hpp>
#include <stfy/sgfx.hpp>
#include <stfy/fmt.hpp>
#include <stfy/var.hpp>
#include <stfy/hal.hpp>


static int bmp_to_sg_bmap(const char * sg_dest, const char * src, const DisplayPalette & palette);
static int match_pixel_to_palette(u8 * pixel, const DisplayPalette & palette);

int main(int argc, char * argv[]){

	//use command line arguments to specify what files to convert
	Cli cli(argc, argv);

	DisplayPalette palette;
	u8 color_palette[3*(1<<sg_api()->bits_per_pixel)];

	palette.set_pixel_format(DisplayPalette::PIXEL_FORMAT_RGB888);
	palette.set_count((1<<sg_api()->bits_per_pixel));
	palette.set_colors(color_palette);
	palette.set_pixel_size(3);

	palette.set_color(0, 0xff, 0xff, 0xff);
	palette.set_color(1, 0xfe, 0xfe, 0xfe);

	bmp_to_sg_bmap("/home/streamz-logo.sgb", "/home/streamz-logo.bmp", palette);

	printf("hi\n");

	Bitmap canvas;

	canvas.load("/home/streamz-logo.sgb");


	canvas.show();

	return 0;
}



//convert a windows bitmap file to a stratify graphics bitmap file
int bmp_to_sg_bmap(const char * sg_dest, const char * src, const DisplayPalette & palette){
	int i,j;
	Bmp bmp(src);
	int bytes_per_pixel;
	sg_color_t color;
	if( bmp.fileno() < 0){
		printf("Failed to open %s\n", src);
		return -1;
	}


	bytes_per_pixel = bmp.bits_per_pixel()/8;

	if( bytes_per_pixel != 3 ){
		printf("Bad BMP format. Format must be R8G8B8\n");
		return -1;
	}
	Bitmap b(bmp.w(), bmp.h());
	b.clear();
	u8 pixel[bytes_per_pixel];

	//import the BMP image file to the bitmap
	for(i=0; i < bmp.h(); i++){
		bmp.seek_row(i);
		for(j=0; j < bmp.w(); j++){
			bmp.read_pixel(pixel, bytes_per_pixel);
			color = match_pixel_to_palette(pixel, palette);
			b.set_pen_color(color);
			b.draw_pixel(sg_point(j,i));
		}
	}

	b.show();

	if( b.save(sg_dest) < 0 ){
		printf("Failed to save bitmap %s\n", sg_dest);
	}

	bmp.close();

	return 0;
}

int match_pixel_to_palette(u8 * pixel, const DisplayPalette & palette){
	u8 i;
	u8 * color;
	u32 pixel1, pixel2;
	u8 closest;
	s32 diff_closest;
	s32 diff;

	pixel1 = (pixel[0] << 16) | (pixel[1] << 8) | pixel[2];

	diff_closest = INT_MAX;
	closest = 0;

	for(i=0; i < palette.count(); i++){
		color = palette.color(i);
		pixel2 = (color[0] << 16) | (color[1] << 8) | color[2];
		diff = pixel2 - pixel1;
		if( abs(diff) < abs(diff_closest) ){
			diff_closest = diff;
			closest = i;
		}
	}

	return closest;
}
