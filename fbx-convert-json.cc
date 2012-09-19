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
  bool firstTake = true;

  printf ("{\"meshes\":[");

  for (auto mesh : model.meshes)
    {
      if (!firstMesh)
        putchar (',');

      putchar ('{');

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

          if (mesh.bindPose.empty ())
            {
              printf ("%.7g, %.7g, %.7g, %.7g, %.7g",
                      mesh.xyz[i * 3 + 0],
                      mesh.xyz[i * 3 + 1],
                      mesh.xyz[i * 3 + 2],
                      mesh.uv[i * 2 + 0],
                      mesh.uv[i * 2 + 1]);
            }
          else
            {
              printf ("%.7g, %.7g, %.7g, %.7g, %.7g, %.7g, %.7g, %.7g, %.7g, %.7g, %.7g, %.7g, %.7g",
                      mesh.xyz[i * 3 + 0],
                      mesh.xyz[i * 3 + 1],
                      mesh.xyz[i * 3 + 2],
                      mesh.uv[i * 2 + 0],
                      mesh.uv[i * 2 + 1],
                      mesh.weights[i * 4 + 0] / 255.0,
                      mesh.weights[i * 4 + 1] / 255.0,
                      mesh.weights[i * 4 + 2] / 255.0,
                      mesh.weights[i * 4 + 3] / 255.0,
                      (double) mesh.bones[i * 4 + 0],
                      (double) mesh.bones[i * 4 + 1],
                      (double) mesh.bones[i * 4 + 2],
                      (double) mesh.bones[i * 4 + 3]);
            }
        }

      printf ("], \"bind-pose\":[");

      for (size_t i = 0; i < mesh.bindPose.size (); ++i)
        {
          if (i)
            putchar (',');

          printf ("%.7g", mesh.bindPose[i]);
        }

      printf ("], \"min-bounds\":[");

      printf ("%.7g, %.7g, %.7g",
              mesh.boundsMin.v[0],
              mesh.boundsMin.v[1],
              mesh.boundsMin.v[2]);

      printf ("], \"max-bounds\":[");

      printf ("%.7g, %.7g, %.7g",
              mesh.boundsMax.v[0],
              mesh.boundsMax.v[1],
              mesh.boundsMax.v[2]);

      printf ("]");

      putchar ('}');

      firstMesh = false;
    }

  printf ("],\"takes\":[");

  for (auto take : model.takes)
    {
      bool firstV = true;

      if (!firstTake)
        putchar (',');

      printf ("{\"name\":\"%s\",\"frames\":[", take.name.c_str());

      for (auto frame : take.frames)
        {
          for (float v : frame.pose)
            {
              if (!firstV)
                putchar(',');

              printf ("%.7g", v);

              firstV = false;
            }
        }

      printf ("]}");

      firstTake = false;
    }

  printf ("]}");
}
