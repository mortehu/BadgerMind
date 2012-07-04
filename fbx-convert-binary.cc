#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <map>
#include <vector>

#include <assert.h>

#include "fbx-convert.h"

static void
FbxConvert_EmitByte (FILE *output, unsigned int byte)
{
  fprintf (output, "%02x", byte);
}

static void
FbxConvert_EmitU16 (FILE *output, unsigned int u16)
{
  fprintf (output, "%02x%02x", u16 & 0xff, (u16 >> 8) & 0xff);
}

static void
FbxConvert_EmitFloat (FILE *output, float value)
{
  union
    {
      float float_;
      unsigned char bytes[4];
    } tmp;

  tmp.float_ = value;

  fprintf (output, "%02x%02x%02x%02x", tmp.bytes[0], tmp.bytes[1], tmp.bytes[2], tmp.bytes[3]);
}

static void
FbxConvert_EmitVertexBuffer (FILE *output, const fbx_mesh &mesh)
{
  size_t i, vertexCount;

  fprintf (output, "(vertex-buffer");

  vertexCount = mesh.xyz.size () / 3;

  assert (vertexCount * 2 == mesh.uv.size ());

  if (!mesh.weights.size ())
    {
      fprintf (output, " data:data(");

      for (i = 0; i < vertexCount; ++i)
        {
          FbxConvert_EmitFloat (output, mesh.xyz[i * 3 + 0]);
          FbxConvert_EmitFloat (output, mesh.xyz[i * 3 + 1]);
          FbxConvert_EmitFloat (output, mesh.xyz[i * 3 + 2]);
          FbxConvert_EmitFloat (output, mesh.uv[i * 2 + 0]);
          FbxConvert_EmitFloat (output, mesh.uv[i * 2 + 1]);
        }

      fprintf (output, " )");
    }
  else
    {
      assert (vertexCount * 4 == mesh.bones.size ());
      assert (mesh.weights.size () == mesh.bones.size ());

      fprintf (output, " data:data(");

      for (i = 0; i < vertexCount; ++i)
        {
          FbxConvert_EmitFloat (output, mesh.xyz[i * 3 + 0]);
          FbxConvert_EmitFloat (output, mesh.xyz[i * 3 + 1]);
          FbxConvert_EmitFloat (output, mesh.xyz[i * 3 + 2]);
          FbxConvert_EmitFloat (output, mesh.uv[i * 2 + 0]);
          FbxConvert_EmitFloat (output, mesh.uv[i * 2 + 1]);

          FbxConvert_EmitByte (output, mesh.weights[i * 4 + 0]);
          FbxConvert_EmitByte (output, mesh.weights[i * 4 + 1]);
          FbxConvert_EmitByte (output, mesh.weights[i * 4 + 2]);
          FbxConvert_EmitByte (output, mesh.weights[i * 4 + 3]);
          FbxConvert_EmitByte (output, mesh.bones[i * 4 + 0]);
          FbxConvert_EmitByte (output, mesh.bones[i * 4 + 1]);
          FbxConvert_EmitByte (output, mesh.bones[i * 4 + 2]);
          FbxConvert_EmitByte (output, mesh.bones[i * 4 + 3]);
        }

      fprintf (output, ")");
    }

  fprintf (output, ")");
}

static void
FbxConvert_EmitIndexBuffer (FILE *output, const fbx_mesh &mesh)
{
  fprintf (output, "(index-buffer");
  fprintf (output, " data:data(");

  for (auto index : mesh.indices)
    FbxConvert_EmitU16 (output, index);

  fprintf (output, ")");
  fprintf (output, ")");
}

static void
FbxConvert_EmitMesh (FILE *output, const fbx_mesh &mesh)
{
  size_t i;

  fprintf (output, "(mesh");
  fprintf (output, " diffuse-texture:(texture-2d URI:\"%s\")", mesh.diffuseTexture.c_str ());
  fprintf (output, " vertex-buffer:");
  FbxConvert_EmitVertexBuffer (output, mesh);
  fprintf (output, " index-buffer:");
  FbxConvert_EmitIndexBuffer (output, mesh);
  fprintf (output, " index-count:%u", (unsigned int) mesh.indices.size ());

  fprintf (output, " matrix:(matrix data:data(");
  for (i = 0; i < 16; ++i)
    FbxConvert_EmitFloat (output, mesh.matrix.v[i]);
  fprintf (output, "))");

  if (mesh.bindPose.size ())
    {
      fprintf (output, " bind-pose:data(");
      for (float v : mesh.bindPose)
        FbxConvert_EmitFloat (output, v);
      fprintf (output, ")");
    }

  fprintf (output, ")");
}

static void
FbxConvert_EmitFrame (FILE *output, const fbx_frame &frame)
{
  for (float v : frame.pose)
    FbxConvert_EmitFloat (output, v);
}

static void
FbxConvert_EmitTake (FILE *output, const fbx_take &take)
{
  if (take.frames.empty ())
    return;

  if (take.frames.front ().pose.empty ())
    return;

  /* XXX: Escape backslashes */

  fprintf (output, "(take");
  fprintf (output, " name:\"%s\"", take.name.c_str ());
  fprintf (output, " interval:%.3g", take.interval);
  fprintf (output, " frames:data(");

  for (auto frame : take.frames)
    FbxConvert_EmitFrame (output, frame);

  fprintf (output, " )"); /* frames */
  fprintf (output, ")"); /* take */
}

void
FbxConvertExportBinary (fbx_model &model)
{
  FILE *pipe;

  pipe = popen (BINDIR "/bm-script-convert", "w");

  for (auto mesh : model.meshes)
    FbxConvert_EmitMesh (pipe, mesh);

  for (auto take : model.takes)
    FbxConvert_EmitTake (pipe, take);
}
