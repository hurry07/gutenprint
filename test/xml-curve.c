#include <gimp-print/gimp-print.h>

int main(int argc, char *argv[])
{
  stp_curve_t curve;

  if (argc != 2)
    {
      fprintf(stderr, "Usage: %s filename.xml\n", argv[0]);
      return 1;
    }

  stp_init();

#ifdef DEBUG
  fprintf(stderr, "stp-xml-parse: reading  `%s'...\n", file);
#endif

  fprintf(stderr, "Using file: %s\n", argv[1]);
  curve = stp_curve_create_from_file(argv[1]);

  if (curve)
    {
      char *output;
      if ((stp_curve_write(stdout, curve)) == 0)
        fprintf(stderr, "curve successfully created\n");
      else
	fprintf(stderr, "error creating curve\n");
      output = stp_curve_write_string(curve);
      if (output)
	{
	  fprintf(stderr, "%s", output);
	  fprintf(stderr, "curve string successfully created\n");
	  stp_free(output);
	}
      else
	fprintf(stderr, "error creating curve string\n");
      stp_curve_free(curve);
    }
  else
    printf("curve is NULL!\n");

  return 0;
}
