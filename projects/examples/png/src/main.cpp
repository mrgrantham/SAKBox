#include <fmt/core.h>

int main(int argc, char **argv) {

  fmt::print("test it\n");
  FILE *fp = fopen(file_name, "rb");
  if (!fp)
    return;
  fread(header, 1, number, fp);
  is_png = !png_sig_cmp(header, 0, number);
  if (!is_png)
    return;

  png_structp png_ptr =
      png_create_read_struct(PNG_LIBPNG_VER_STRING, (png_voidp)user_error_ptr,
                             user_error_fn, user_warning_fn);
  if (!png_ptr)
    return;

  png_infop info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr) {
    png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
    return;
  }

  png_infop end_info = png_create_info_struct(png_ptr);
  if (!end_info) {
    png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
    return;
  }

  // png_structp png_ptr = png_create_read_struct_2
  //        (PNG_LIBPNG_VER_STRING, (png_voidp)user_error_ptr,
  //         user_error_fn, user_warning_fn, (png_voidp)
  //         user_mem_ptr, user_malloc_fn, user_free_fn);

  return EXIT_SUCCESS;
}