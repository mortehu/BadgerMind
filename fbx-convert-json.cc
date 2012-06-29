#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sysexits.h>
#include <unistd.h>

#include "fbx-convert.h"

void
FbxConvertExportJSON (fbx_model &model)
{
  bool firstMesh = true;

  putchar ('[');

  for (auto mesh : model.meshes)
    {
      if (!firstMesh)
        putchar (',');

      printf ("{\"type\":\"mesh\",");

      if (mesh.diffuseTexture.length ())
        printf ("\"texture-URI\":\"%s\",\n", mesh.diffuseTexture.c_str ());

      printf ("\"matrix\":[");

      for (size_t i = 0; i < 16; ++i)
        {
          if (i)
            putchar (',');

          printf (" %.7g", mesh.matrix.v[i]);
        }

      printf ("], \"triangles\":[");

      bool firstIndex = true;

      for (auto index : mesh.indices)
        {
          if (!firstIndex)
            putchar (',');

          printf ("%u", (unsigned int) index);

          firstIndex = false;
        }

      printf ("], \"vertices\":[");

      for (size_t i = 0; i < mesh.xyz.size () / 3; ++i)
        {
          if (i)
            putchar (',');

          printf ("%.7g, %.7g, %.7g, %.7g, %.7g",
                  mesh.xyz[i * 3 + 0],
                  mesh.xyz[i * 3 + 1],
                  mesh.xyz[i * 3 + 2],
                  mesh.uv[i * 2 + 0],
                  mesh.uv[i * 2 + 1]);
        }

      printf ("]");

      putchar ('}');

      firstMesh = false;
    }

  putchar (']');
}
