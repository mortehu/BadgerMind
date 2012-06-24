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
PrintTransf (const char *name, int index, const FbxAMatrix& matrix)
{
  FbxQuaternion quat;
  FbxVector4 transl;

  transl = matrix.GetT ();
  quat = matrix.GetQ ();

  printf ("%s %d  %.6g %.6g %.6g %.6g  %.6g %.6g %.6g ", name, index,
          quat[0], quat[1], quat[2], quat[3],
          transl[0], transl[1], transl[2]);

  printf ("\n");
}

static void
PrintMatrix (const char *name, int index, const FbxAMatrix& matrix)
{
  int k;

  printf ("%s %d", name, index);

  for (k = 0; k < 12; ++k)
    printf (" %.6g", matrix.Get (k / 4, k % 4));

  for (; k < 16; ++k)
    printf (" %.6g", matrix.Get (k / 4, k % 4));

  printf ("\n");
}

static void
ExportClusterDeformation(FbxAMatrix& pGlobalPosition,
                         FbxMesh* pMesh,
                         const std::vector<vertex> &vertexArray)
{
  FbxCluster::ELinkMode lClusterMode = ((FbxSkin*)pMesh->GetDeformer(0, FbxDeformer::eSkin))->GetCluster(0)->GetLinkMode();

  unsigned int i, j;
  unsigned int lClusterCount=0;
  unsigned int lVertexCount = pMesh->GetControlPointsCount();
  unsigned int lSkinCount=pMesh->GetDeformerCount(FbxDeformer::eSkin);

  VertexWeights *weights;

  weights = new VertexWeights[lVertexCount];

  for(i=0; i<lSkinCount; ++i)
    {
      lClusterCount = ((FbxSkin *)pMesh->GetDeformer(i, FbxDeformer::eSkin))->GetClusterCount();

      for (j=0; j<lClusterCount; ++j)
        {
          FbxCluster* lCluster = ((FbxSkin *) pMesh->GetDeformer(i, FbxDeformer::eSkin))->GetCluster(j);
          int k;

          if (!lCluster->GetLink())
            continue;

          FbxAMatrix lReferenceGlobalInitPosition;
          FbxAMatrix lClusterGlobalInitPosition;
          FbxAMatrix lClusterGeometry;

          FbxAMatrix lClusterRelativeInitPosition;
          FbxAMatrix lVertexTransformMatrix;

          if (lClusterMode == FbxCluster::eAdditive && lCluster->GetAssociateModel())
            {
              lCluster->GetTransformAssociateModelMatrix(lReferenceGlobalInitPosition);
            }
          else
            {
              lCluster->GetTransformMatrix(lReferenceGlobalInitPosition);

              lReferenceGlobalInitPosition *= GetGeometry(pMesh->GetNode());
            }

          lCluster->GetTransformLinkMatrix(lClusterGlobalInitPosition);

          lClusterRelativeInitPosition = lClusterGlobalInitPosition.Inverse() * lReferenceGlobalInitPosition;

          PrintMatrix ("bone-matrix", j, lClusterRelativeInitPosition);

          int lVertexIndexCount = lCluster->GetControlPointIndicesCount();

          for (k = 0; k < lVertexIndexCount; ++k)
            {
              int lIndex = lCluster->GetControlPointIndices()[k];
              double lWeight = lCluster->GetControlPointWeights()[k];

              if (lWeight == 0.0)
                continue;

              weights[lIndex].AddWeight (lWeight, j);
            }
        }
    }

  for (i = 0; i < vertexArray.size (); i++)
  {
    unsigned int controlPoint;

    controlPoint = vertexArray[i].controlPoint;

    printf ("bone-weights %d %.6g %.6g %.6g %.6g %u %u %u %u\n",
        i,
        weights[controlPoint].weights[0], weights[controlPoint].weights[1], weights[controlPoint].weights[2], weights[controlPoint].weights[3],
        weights[controlPoint].bones[0], weights[controlPoint].bones[1], weights[controlPoint].bones[2], weights[controlPoint].bones[3]);
  }

  delete [] weights;
}

static void
ExportMesh (FbxNode* node, FbxAMatrix& pGlobalPosition)
{
  FbxMesh* mesh;
  unsigned int lClusterCount = 0;
  unsigned int lSkinCount = 0;
  unsigned int lVertexCount;
  unsigned int i;

  std::map<vertex, int> vertexMap;
  std::vector<vertex> vertexArray;

  mesh = node->GetMesh ();
  lVertexCount = mesh->GetControlPointsCount();

  if (!lVertexCount)
    return;

  lSkinCount = mesh->GetDeformerCount(FbxDeformer::eSkin);

  for (i = 0; i < lSkinCount; i++)
    lClusterCount += ((FbxSkin *)(mesh->GetDeformer(i, FbxDeformer::eSkin)))->GetClusterCount();

  printf ("begin-mesh\n");

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

              printf ("texture %s\n", (const char *) lFileTexture->GetRelativeFileName ());
            }
        }
    }

  printf ("matrix");
  for (i = 0; i < 16; ++i)
    printf (" %.6g", pGlobalPosition.Get (i / 4, i % 4));
  printf ("\n");

  FbxStringList lUVNames;
  const char * lUVName;

  mesh->GetUVSetNames(lUVNames);

  if (lUVNames.GetCount())
    lUVName = lUVNames[0];
  else
    lUVName = 0;

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
        printf ("triangle %d %d %d\n", indices[0], indices[j - 1], indices[j]);

      delete [] indices;
    }

  for (i = 0; i < vertexArray.size (); ++i)
    {
      vertex v;
      double *xyz;

      v = vertexArray[i];

      xyz = (double *) mesh->GetControlPoints()[v.controlPoint];

      printf ("xyz %d %.6g %.6g %.6g\n", i, xyz[0], xyz[1], xyz[2]);
      printf ("uv %d %.6g %.6g\n", i, v.u, v.v);
    }

  if (lClusterCount)
    ExportClusterDeformation (pGlobalPosition, mesh, vertexArray);

  printf ("end-mesh\n");

  fflush (stdout);
}


void
ExportMeshBinaryRecursive (FbxNode* node,
                           FbxAMatrix& pParentGlobalPosition)
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
    ExportMesh (node, lGlobalPosition);

  int i, lCount = node->GetChildCount();

  for (i = 0; i < lCount; i++)
    ExportMeshBinaryRecursive (node->GetChild(i), lGlobalPosition);
}

// Get the geometry deformation local to a node. It is never inherited by the
// children.
static FbxAMatrix
GetGeometry(FbxNode* node)
{
  const FbxVector4 T = node->GetGeometricTranslation(FbxNode::eSourcePivot);
  const FbxVector4 R = node->GetGeometricRotation(FbxNode::eSourcePivot);
  const FbxVector4 S = node->GetGeometricScaling(FbxNode::eSourcePivot);

  return FbxAMatrix(T, R, S);
}

static void ExportClusterDeformation(FbxAMatrix& pGlobalPosition,
                              FbxMesh* pMesh,
                              FbxTime& time)
{
  FbxCluster::ELinkMode lClusterMode;
  FbxSkin *skin;

  skin = (FbxSkin *) pMesh->GetDeformer(0, FbxDeformer::eSkin);

  if (!skin)
    return;

  lClusterMode = skin->GetCluster(0)->GetLinkMode();

  int i, j;
  int lClusterCount=0;
  int lVertexCount = pMesh->GetControlPointsCount();
  int lSkinCount=pMesh->GetDeformerCount(FbxDeformer::eSkin);

  if (!lVertexCount)
    return;

  for(i=0; i<lSkinCount; ++i)
    {
      lClusterCount =( (FbxSkin *)pMesh->GetDeformer(i, FbxDeformer::eSkin))->GetClusterCount();

      for (j=0; j<lClusterCount; ++j)
        {
          FbxCluster* lCluster =((FbxSkin *) pMesh->GetDeformer(i, FbxDeformer::eSkin))->GetCluster(j);

          if (!lCluster->GetLink())
            continue;

          FbxAMatrix lReferenceGlobalCurrentPosition;
          FbxAMatrix lClusterGlobalInitPosition;
          FbxAMatrix lClusterGlobalCurrentPosition;
          FbxAMatrix lClusterGeometry;

          FbxAMatrix lClusterRelativeInitPosition;
          FbxAMatrix lClusterRelativeCurrentPositionInverse;
          FbxAMatrix lVertexTransformMatrix;

          if (lClusterMode == FbxCluster::eAdditive && lCluster->GetAssociateModel())
            {
              lReferenceGlobalCurrentPosition = lCluster->GetAssociateModel()->EvaluateGlobalTransform(time);

              lReferenceGlobalCurrentPosition *= GetGeometry(lCluster->GetAssociateModel());
            }
          else
            {
              lReferenceGlobalCurrentPosition = pGlobalPosition;
            }


          lClusterGlobalCurrentPosition = lCluster->GetLink()->EvaluateGlobalTransform(time);

          lClusterRelativeCurrentPositionInverse = lReferenceGlobalCurrentPosition.Inverse() * lClusterGlobalCurrentPosition;

          PrintTransf ("bone-transf", j, lClusterRelativeCurrentPositionInverse);
        }
    }
}

static void
ExportAnimRecursive(FbxNode* node,
                    FbxTime& time,
                    FbxAMatrix& pParentGlobalPosition)
{
  FbxAMatrix lGlobalPosition, lGeometryOffset, lGlobalOffPosition;
  FbxNodeAttribute* lNodeAttribute;
  int i, count;

  lGlobalPosition = node->EvaluateGlobalTransform(time);
  lGeometryOffset = GetGeometry(node);
  lGlobalOffPosition = lGlobalPosition * lGeometryOffset;

  lNodeAttribute = node->GetNodeAttribute();

  if (lNodeAttribute
      && lNodeAttribute->GetAttributeType() == FbxNodeAttribute::eMesh)
    {
      ExportClusterDeformation (lGlobalPosition, (FbxMesh *) node->GetNodeAttribute(), time);
    }

  count = node->GetChildCount();

  for (i = 0; i < count; ++i)
    ExportAnimRecursive(node->GetChild(i), time, lGlobalPosition);
}

static void
ExportAnimScene (FbxScene* pScene, FbxTime& time)
{
  FbxAMatrix lDummyGlobalPosition;

  int i, lCount = pScene->GetRootNode()->GetChildCount();

  printf ("begin-frame\n");
  printf ("time %.3f\n", time.GetSecondDouble());

  for (i = 0; i < lCount; i++)
    ExportAnimRecursive(pScene->GetRootNode()->GetChild(i), time, lDummyGlobalPosition);

  printf ("end-frame\n");
}

void
ExportTakeBinary (FbxScene *scene, FbxString *takeName)
{
  FbxTakeInfo* lCurrentTakeInfo;
  FbxTime period, start, stop, currentTime;

  FbxAnimStack *lCurrentAnimationStack;

  lCurrentAnimationStack = scene->FindMember(FBX_TYPE(FbxAnimStack), takeName->Buffer());

  if (lCurrentAnimationStack == NULL)
    {
      fprintf (stderr, "Animation stack %s not found!\n", (const char *) *takeName);

      exit (EXIT_FAILURE);
    }

  scene->GetEvaluator()->SetContext(lCurrentAnimationStack);

  lCurrentTakeInfo = scene->GetTakeInfo(*takeName);

  if (!lCurrentTakeInfo)
    return;

  printf ("begin-take %s\n", (const char *) *takeName);

  period.SetMilliSeconds (10.0);

  start = lCurrentTakeInfo->mLocalTimeSpan.GetStart();
  stop = lCurrentTakeInfo->mLocalTimeSpan.GetStop();

  for (currentTime = start; currentTime <= stop; currentTime += period)
    ExportAnimScene (scene, currentTime);

  printf ("end-take\n");
}
