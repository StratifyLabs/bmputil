
#define __ARM_ARCH_7M__ 1


#include <cstdio>

#include <sos/dev/display.h>
#include <sapi/sys.hpp>
#include <sapi/sgfx.hpp>
#include <sapi/fmt.hpp>
#include <sapi/var.hpp>
#include <sapi/hal.hpp>

typedef struct MCU_PACK {
	u16 w;
	u16 h;
	u32 size;
} mg_bitmap_header_t;

static int son_to_palette(const char * dest, const char * src);
static int bmp_to_sg_bmap(const char * sg_dest, const char * src, const DisplayPalette & palette);
static int match_pixel_to_palette(u8 * pixel, const DisplayPalette & palette);
static void show_usage(const Cli & cli);
static int convert_mbm(const String & input, const String & output);

int main(int argc, char * argv[]){
	String input_path;
	String output_path;
	String palette_path;
	Cli cli(argc, argv);

	cli.handle_version();

	if( cli.is_option("-sgp") ){
		input_path = cli.get_option_argument("-i");
		output_path = cli.get_option_argument("-o");

		if( input_path == "" ){
			printf("Failed to get input path\n");
			show_usage(cli);
			exit(1);
		}

		if( output_path == "" ){
			printf("Failed to get output path\n");
			show_usage(cli);
			exit(1);
		}

		son_to_palette(output_path, input_path);
		exit(1);
	}

	input_path = cli.get_option_argument("-i");
	output_path = cli.get_option_argument("-o");
	palette_path = cli.get_option_argument("-p");

	if( cli.is_option("-mbm2sgb") ){
		printf("Converting %s to %s\n", input_path.c_str(), output_path.c_str());
		convert_mbm(input_path, output_path);
		exit(0);
	}

	if( cli.is_option("-show") ){
		Bitmap b;

		input_path = cli.get_option_argument("-show");
		if( b.load(input_path) < 0 ){
			printf("Failed to load: '%s'\n", input_path.c_str());
			exit(1);
		}
		printf("Size header is %d\n", sizeof(sg_bmap_header_t));
		printf("Bitmap %s (%dx%d):\n", input_path.c_str(), b.width(), b.height());
		b.show();
		exit(0);
	}

	DisplayPalette palette;

	if( cli.is_option("-mono") ){

		//use a monochrome palette
		palette.set_monochrome();

	} else {
		if( palette.load(palette_path) < 0 ){
			printf("Failed to load palette: '%s'\n", palette_path.c_str());
			show_usage(cli);
			exit(1);
		}
	}


	if( bmp_to_sg_bmap(output_path.c_str(), input_path.c_str(), palette) < 0 ){
		show_usage(cli);
	}

	return 0;
}



//convert a windows bitmap file to a stratify graphics bitmap file
int bmp_to_sg_bmap(const char * sg_dest, const char * src, const DisplayPalette & palette){
	int i,j;
	Bmp bmp(src);
	int bytes_per_pixel;
	sg_color_t color;
	if( bmp.fileno() < 0){
		printf("Failed to open: '%s'\n", src);
		return -1;
	}

	bytes_per_pixel = bmp.bits_per_pixel()/8;

	if( bytes_per_pixel != 3 ){
		printf("Bad BMP format. Format must be R8G8B8\n");
		return -1;
	}
	Bitmap b(bmp.width(), bmp.height());
	b.clear();
	u8 pixel[bytes_per_pixel];

	//import the BMP image file to the bitmap
	for(i=0; i < bmp.height(); i++){
		bmp.seek_row(i);
		for(j=0; j < bmp.width(); j++){
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

int son_to_palette(const char * dest, const char * src){
	Son<4> son;
	String value;
	DisplayPalette palette;
	int i;
	String access;
	u8 count;
	u8 pixel_size;
	int err;


	if( son.open_read(src) < 0 ){
		printf("Failed to open %s (%d)\n", src, son.get_error());
		perror("Operation failed");
		return -1;
	}

	if( son.read_str("type", value.cdata(), value.capacity()) < 0 ){
		printf("Failed to read type %d\n", son.err());
		son.close();
		return -1;
	}

	if( value != "display_palette" ){
		printf("File data type is '%s' not 'display_palette'\n", value.c_str());
		son.close();
		return -1;
	}

	son.read_str("pixel_format", value.cdata(), value.capacity());
	if( value == "RGB888" ){
		printf("Value is RGB888\n");
	}

	son.get_error();
	count = son.read_unum("count");
	pixel_size = son.read_unum("pixel_size");
	if( (err = son.get_error()) > 0 ){
		printf("Error loading count/pixel size (%d)\n", err);
		return -1;
	}

	printf("Count %d %d\n", count, pixel_size);

	if( palette.alloc_colors(count, pixel_size) < 0 ){
		return -1;
	}

	if( value == "RGB888" ){
		printf("Parsing RGB data\n");
		palette.set_pixel_format(DisplayPalette::PIXEL_FORMAT_RGB888);
		for(i=0; i < palette.count(); i++){
			String color_access;
			u8 r, g, b;
			access.sprintf("colors[%d].r", i);
			r = son.read_unum(access.c_str());

			access.sprintf("colors[%d].g", i);
			g = son.read_unum(access);

			access.sprintf("colors[%d].b", i);
			b = son.read_unum(access);

			printf("Add Color: %d,%d,%d\n", r, g, b);
			palette.set_color(i, r, g, b);
		}
	}

	son.close();

	printf("Saving %s\n", dest);
	if( palette.save(dest) < 0 ){
		printf("Failed to save palette %s\n", dest);
		perror("Error:");
		return -1;
	}
	printf("Saved!\n");

	return 0;
}

int match_pixel_to_palette(u8 * pixel, const DisplayPalette & palette){
	u8 i;
	u8 * color;
	u8 closest;
	s16 diff[3];
	s32 diff_sum;
	s32 diff_closest;
	u32 pixel_brightness;


	diff_closest = INT_MAX;
	closest = 0;

	for(i=0; i < palette.count(); i++){
		color = palette.color(i);

		if( palette.pixel_size() == 1 ){
			//palette is just one pixel
			pixel_brightness = (pixel[0] + pixel[1] + pixel[2])/3;
			diff[0] = color[0] - pixel_brightness;
			diff[1] = 0;
			diff[2] = 0;
		} else {
			diff[0] = color[0] - pixel[0];
			diff[1] = color[1] - pixel[1];
			diff[2] = color[2] - pixel[2];
		}

		diff_sum = diff[0] + diff[1] + diff[2];
		if( abs(diff_sum) < abs(diff_closest) ){
			diff_closest = diff_sum;
			closest = i;
		}
	}

	return closest;
}

int convert_mbm(const String & input, const String & output){
	File input_file;
	File output_file;
	mg_bitmap_header_t header;
	Bitmap b;
	Bitmap after;
	u8 byte;
	int j;
	int i;
	u8 * bitmap_data;
	u32 bitmap_width;
	u32 reverse;

	if( input_file.open(input, File::RDONLY) < 0 ){
		printf("Failed to open input: '%s'\n", input.c_str());
		return -1;
	}

	if( input_file.read(&header, sizeof(header)) == sizeof(header) ){
		printf("W: %d\n", header.w);
		printf("H: %d\n", header.h);
		printf("Size: %ld\n", header.size);

		bitmap_width = (header.w/32 + 1)*32;
		b.alloc(header.w, header.h);
		bitmap_data = (u8*)b.data();

		memset(b.data(), 0, b.capacity());

		for(j=0; j < header.h; j++){
			for(i=0; i < header.w/8; i++){
				input_file.read(&byte, 1);
				reverse = __REV(byte);
				bitmap_data[i + j*bitmap_width/8] = __RBIT(reverse);
			}
		}

		b.show();

		if( b.save(output) < 0 ){
			printf("Failed to save: '%s'\n", output.c_str());
		} else {
			printf("Saved %s as Stratify Graphics Bitmap using %dbpp\n", output.c_str(), sg_api()->bits_per_pixel);
		}

		b.clear();

		after.load(output);

		after.show();



		input_file.close();
	}

	return 0;
}

void show_usage(const Cli & cli){
	printf("Usage: %s -i <bmp_file> -o <output> [-p <palette>|-mono]\n", cli.name());
	printf("\t%s -show <sgb_file>\n", cli.name());
	printf("\t%s -mbm2sgb -i <mbm_file> -o <sgb_file>\n", cli.name());
}
