#include <map>
#include <vector>

#include <assert.h>

#include "fbx-convert.h"

static off_t FbxConvert_dumpOffset;

static void
FbxConvert_EmitByte (unsigned int byte)
{
  putchar (byte & 0xff);
  FbxConvert_dumpOffset++;
}

static void
FbxConvert_EmitU16 (unsigned int u16)
{
  putchar (u16 & 0xff);
  putchar ((u16 >> 8) & 0xff);
  FbxConvert_dumpOffset += 2;
}

static void
FbxConvert_EmitU32 (unsigned int u32)
{
  putchar (u32 & 0xff);
  putchar ((u32 >> 8) & 0xff);
  putchar ((u32 >> 16) & 0xff);
  putchar ((u32 >> 24) & 0xff);
  FbxConvert_dumpOffset += 4;
}

static void
FbxConvert_EmitFloat (float value)
{
  fwrite (&value, sizeof (value), 1, stdout);
  FbxConvert_dumpOffset += sizeof (float);
}

static void
FbxConvert_EmitPointer (off_t byte)
{
  putchar (byte & 0xff);
  putchar ((byte >> 8) & 0xff);
  putchar ((byte >> 16) & 0xff);
  putchar ((byte >> 24) & 0xff);

  FbxConvert_dumpOffset += 4;
}

static void
FbxConvert_Align (void)
{
  while (FbxConvert_dumpOffset & 0x3)
    FbxConvert_EmitByte (0xaa);
}

static off_t
FbxConvert_EmitVertexBuffer (const fbx_mesh &mesh)
{
  size_t i, vertexCount;
  off_t result;

  vertexCount = mesh.xyz.size () / 3;

  assert (vertexCount * 2 == mesh.uv.size ());

  FbxConvert_Align ();

  result = FbxConvert_dumpOffset;

  if (!mesh.weights.size ())
    {
      assert (!mesh.bones.size ());

      FbxConvert_EmitU16 (0x0000); /* Vertex format */
      FbxConvert_EmitU16 (vertexCount);

      for (i = 0; i < vertexCount; ++i)
        {
          FbxConvert_EmitFloat (mesh.xyz[i * 3 + 0]);
          FbxConvert_EmitFloat (mesh.xyz[i * 3 + 1]);
          FbxConvert_EmitFloat (mesh.xyz[i * 3 + 2]);
          FbxConvert_EmitFloat (mesh.uv[i * 3 + 0]);
          FbxConvert_EmitFloat (mesh.uv[i * 3 + 1]);
        }
    }
  else
    {
      assert (vertexCount * 4 == mesh.bones.size ());
      assert (mesh.weights.size () == mesh.bones.size ());

      FbxConvert_EmitU16 (0x0001); /* Vertex format */
      FbxConvert_EmitU16 (vertexCount);

      for (i = 0; i < vertexCount; ++i)
        {
          FbxConvert_EmitFloat (mesh.xyz[i * 3 + 0]);
          FbxConvert_EmitFloat (mesh.xyz[i * 3 + 1]);
          FbxConvert_EmitFloat (mesh.xyz[i * 3 + 2]);
          FbxConvert_EmitFloat (mesh.uv[i * 3 + 0]);
          FbxConvert_EmitFloat (mesh.uv[i * 3 + 1]);

          FbxConvert_EmitByte (mesh.weights[i * 4 + 0]);
          FbxConvert_EmitByte (mesh.weights[i * 4 + 1]);
          FbxConvert_EmitByte (mesh.weights[i * 4 + 2]);
          FbxConvert_EmitByte (mesh.weights[i * 4 + 3]);
          FbxConvert_EmitByte (mesh.bones[i * 4 + 0]);
          FbxConvert_EmitByte (mesh.bones[i * 4 + 1]);
          FbxConvert_EmitByte (mesh.bones[i * 4 + 2]);
          FbxConvert_EmitByte (mesh.bones[i * 4 + 3]);
        }
    }

  return result;
}

static off_t
FbxConvert_EmitIndexBuffer (const fbx_mesh &mesh)
{
  off_t result;

  FbxConvert_Align ();

  result = FbxConvert_dumpOffset;

  FbxConvert_EmitU32 (mesh.indices.size());

  for (auto index : mesh.indices)
    FbxConvert_EmitU16 (index);

  return result;
}

static off_t
FbxConvert_EmitString (const char *string)
{
  off_t result;

  result = FbxConvert_dumpOffset;

  while (*string)
    FbxConvert_EmitByte (*string++);

  FbxConvert_EmitByte (0);

  return result;
}

static off_t
FbxConvert_EmitMesh (const fbx_mesh &mesh, off_t nextMesh)
{
  size_t i;
  off_t vertexBufferOffset, indexBufferOffset, diffuseTextureNameOffset, result;

  diffuseTextureNameOffset = FbxConvert_EmitString (mesh.diffuseTexture.c_str ());
  vertexBufferOffset = FbxConvert_EmitVertexBuffer (mesh);
  indexBufferOffset = FbxConvert_EmitIndexBuffer (mesh);

  FbxConvert_Align ();

  result = FbxConvert_dumpOffset;

  FbxConvert_EmitPointer (nextMesh);
  for (i = 0; i < 16; ++i)
    FbxConvert_EmitFloat (mesh.matrix.v[i]);
  FbxConvert_EmitPointer (diffuseTextureNameOffset);
  FbxConvert_EmitPointer (vertexBufferOffset);
  FbxConvert_EmitPointer (indexBufferOffset);

  return result;
}

static off_t
FbxConvert_EmitFrame (const fbx_frame &frame)
{
  off_t result;

  result = FbxConvert_dumpOffset;

  for (auto v : frame.pose)
    FbxConvert_EmitFloat (v);

  return result;
}

static off_t
FbxConvert_EmitTake (const fbx_take &take, off_t nextTake)
{
  off_t takeNameOffset, result;

  takeNameOffset = FbxConvert_EmitString (take.name.c_str ());

  FbxConvert_Align ();

  result = FbxConvert_dumpOffset;

  FbxConvert_EmitPointer (nextTake);
  FbxConvert_EmitPointer (takeNameOffset);
  FbxConvert_EmitFloat (take.interval);
  FbxConvert_EmitU32 (take.frames.size ());

  for (auto frame : take.frames)
    FbxConvert_EmitFrame (frame);

  return result;
}

void
FbxConvertExportBinary (fbx_model &model)
{
  off_t nextMesh = 0, nextTake = 0;

  FbxConvert_EmitU32 (0xbad6e2aa);

  for (auto mesh : model.meshes)
    nextMesh = FbxConvert_EmitMesh (mesh, nextMesh);

  for (auto take : model.takes)
    nextTake = FbxConvert_EmitTake (take, nextTake);

  FbxConvert_Align ();

  FbxConvert_EmitPointer (nextTake);
  FbxConvert_EmitPointer (nextMesh);
}
