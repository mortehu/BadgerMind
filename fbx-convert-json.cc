#include <map>
#include <vector>

#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sysexits.h>
#include <unistd.h>

#define FBXSDK_NEW_API

#include <fbxsdk.h>
#include <fbxsdk/utils/fbxutilities.h>

static FbxAMatrix
GetGeometry(FbxNode* node);

struct vertex
{
  unsigned int controlPoint;
  float u, v;

  bool
  operator<(const struct vertex &rhs) const
    {
#define TEST(a) if (a != rhs.a) return a < rhs.a;

      TEST(controlPoint)
      TEST(u)

#undef TEST

      return v < rhs.v;
    }
};

struct VertexWeights
{
  double weights[4];
  unsigned int bones[4];

  VertexWeights()
    {
      memset (weights, 0, sizeof (weights));
      memset (bones, 0, sizeof (bones));
    }

  void AddWeight (double weight, unsigned int bone)
    {
      unsigned int i;

      for (i = 0; i < 4; ++i)
        {
          if (weights[i] < weight)
            {
              memmove (weights + i + 1, weights + i, (4 - i - 1) * sizeof (weights[0]));
              memmove (bones + i + 1, bones + i, (4 - i - 1) * sizeof (bones[0]));

              weights[i] = weight;
              bones[i] = bone;

              return;
            }
        }
    }
};

static void
ExportMesh (FbxNode* node, FbxAMatrix& pGlobalPosition, int *first)
{
  FbxMesh* mesh;
  unsigned int lVertexCount;
  unsigned int i;

  std::map<vertex, int> vertexMap;
  std::vector<vertex> vertexArray;

  mesh = node->GetMesh ();
  lVertexCount = mesh->GetControlPointsCount();

  if (!lVertexCount)
    return;

  if (!*first)
    printf (",");

  *first = 0;

  printf ("{\"type\":\"mesh\",");


    {
      int lMaterialIndex;
      int lTextureIndex;
      FbxProperty lProperty;
      int lNbTex;
      FbxSurfaceMaterial *lMaterial = NULL;
      int lNbMat = node->GetSrcObjectCount(FbxSurfaceMaterial::ClassId);

      for (lMaterialIndex = 0; lMaterialIndex < lNbMat; lMaterialIndex++)
        {
          lMaterial = FbxCast <FbxSurfaceMaterial>(node->GetSrcObject(FbxSurfaceMaterial::ClassId, lMaterialIndex));

          if(!lMaterial)
            continue;

          lProperty = lMaterial->FindProperty(FbxSurfaceMaterial::sDiffuse);

          if(!lProperty.IsValid())
            continue;

          lNbTex = lProperty.GetSrcObjectCount(FbxTexture::ClassId);

          for (lTextureIndex = 0; lTextureIndex < lNbTex; lTextureIndex++)
            {
              FbxFileTexture *lFileTexture;

              if (!(lFileTexture = FbxCast<FbxFileTexture>(lProperty.GetSrcObject(FbxTexture::ClassId, lTextureIndex))))
                continue;

              printf ("\"texture-URI\":\"%s\",\n", (const char *) lFileTexture->GetRelativeFileName ());
            }
        }
    }

  printf ("\"matrix\":[");
  for (i = 0; i < 16; ++i)
    {
      if (i)
        putchar (',');
      printf (" %.6g", pGlobalPosition.Get (i / 4, i % 4));
    }
  printf ("],\n");

  FbxStringList lUVNames;
  const char * lUVName;

  mesh->GetUVSetNames(lUVNames);

  if (lUVNames.GetCount())
    lUVName = lUVNames[0];
  else
    lUVName = 0;

  printf ("\"triangles\":[");

  for (i = 0; i < (unsigned int) mesh->GetPolygonCount(); i++)
    {
      int j, polygonSize;
      int *indices;

      polygonSize = mesh->GetPolygonSize(i);

      indices = new int[polygonSize];

      for (j = 0; j < polygonSize; j++)
        {
          vertex newVertex;

          FbxVector2 uv;

          newVertex.controlPoint = mesh->GetPolygonVertex(i, j);

          mesh->GetPolygonVertexUV (i, j, lUVName, uv);

          newVertex.u = uv[0];
          newVertex.v = uv[1];

          auto oldVertex = vertexMap.find (newVertex);

          if (oldVertex != vertexMap.end ())
            indices[j] = oldVertex->second;
          else
            {
              indices[j] = vertexArray.size ();

              vertexArray.push_back (newVertex);
              vertexMap[newVertex] = indices[j];
            }
        }

      for (j = 2; j < polygonSize; ++j)
        {
          if (i || j > 2)
            putchar (',');

          printf ("%d, %d, %d", indices[0], indices[j - 1], indices[j]);
        }

      delete [] indices;
    }

  printf ("],\n");

  printf ("\"vertices\":[");

  for (i = 0; i < vertexArray.size (); ++i)
    {
      vertex v;
      double *xyz;

      v = vertexArray[i];

      xyz = (double *) mesh->GetControlPoints()[v.controlPoint];

      if (i)
        putchar (',');

      printf ("%.6g, %.6g, %.6g, %.6g, %.6g", xyz[0], xyz[1], xyz[2], v.u, v.v);
    }

  printf ("]");

  printf ("}");
}


void
FbxConvert_ExportMeshAsJSON (FbxNode* node,
                             FbxAMatrix& pParentGlobalPosition,
                             int *first)
{
  FbxTime time;

  // Compute the node's global position.
  FbxAMatrix lGlobalPosition = node->EvaluateGlobalTransform(time);

  // Geometry offset.
  // it is not inherited by the children.
  FbxAMatrix lGeometryOffset = GetGeometry(node);
  FbxAMatrix lGlobalOffPosition = lGlobalPosition * lGeometryOffset;

  FbxNodeAttribute* lNodeAttribute = node->GetNodeAttribute();

  if (lNodeAttribute && lNodeAttribute->GetAttributeType() == FbxNodeAttribute::eMesh)
    ExportMesh (node, lGlobalPosition, first);

  int i, lCount = node->GetChildCount();

  for (i = 0; i < lCount; i++)
    FbxConvert_ExportMeshAsJSON (node->GetChild(i), lGlobalPosition, first);
}

static FbxAMatrix
GetGeometry(FbxNode* node)
{
  const FbxVector4 T = node->GetGeometricTranslation(FbxNode::eSourcePivot);
  const FbxVector4 R = node->GetGeometricRotation(FbxNode::eSourcePivot);
  const FbxVector4 S = node->GetGeometricScaling(FbxNode::eSourcePivot);

  return FbxAMatrix(T, R, S);
}
