#include <stdint.h>

#include <map>
#include <string>
#include <vector>

struct fbx_matrix
{
  float v[16];
};

struct fbx_vector
{
  float v[3];
};

struct fbx_take_range
{
  std::string name;
  unsigned int begin, end;
};

struct fbx_mesh
{
  float lod;

  std::string diffuseTexture;

  fbx_vector boundsMin, boundsMax;

  fbx_matrix matrix;
  std::vector<float> bindPose;
  std::vector<unsigned int> indices;
  std::vector<float> xyz;
  std::vector<float> uv;
  std::vector<uint8_t> weights;
  std::vector<uint8_t> bones;
};

struct fbx_frame
{
  std::vector<float> pose;
};

struct fbx_take
{
  std::string name;
  float interval;

  std::vector<fbx_frame> frames;
};

struct fbx_model
{
  std::vector<fbx_mesh> meshes;
  std::vector<fbx_take_range> takeRanges;
  std::vector<fbx_take> takes;
};

void
FbxConvertExportBinary (fbx_model &model, unsigned int pointerSize);

void
FbxConvertExportJSON (fbx_model &model);
