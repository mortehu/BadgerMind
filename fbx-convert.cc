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

static int FbxConvert_printHelp;
static int FbxConvert_printVersion;

static struct option FbxConvert_longOptions[] =
{
    { "help",     no_argument, &FbxConvert_printHelp, 1 },
    { "version",  no_argument, &FbxConvert_printVersion, 1 },
    { 0, 0, 0, 0 }
};

static void
PreparePointCacheData(FbxScene* pScene);

static void
ExportMeshRecursive(FbxNode* node,
                    FbxAMatrix& pParentGlobalPosition);

static void
ExportAnimScene(FbxScene* pScene, FbxTime& time);

static FbxAMatrix
GetGeometry(FbxNode* node);

static void
ConvertNurbsAndPatchRecursive(FbxManager* pSdkManager,
                              FbxNode* node);

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

int
main(int argc, char** argv)
{
  int i;
  int lFileFormat = -1;
  FbxString *gFileName = new FbxString;

  FbxManager* FbxManager;
  FbxImporter* fbxImporter;
  FbxScene* scene;

  FbxArray<FbxString*> takeNames;

  while(-1 != (i = getopt_long(argc, argv, "", FbxConvert_longOptions, NULL)))
    {
      switch(i)
        {
        case '?':

          fprintf(stderr, "Try `%s --help' for more information.\n", argv[0]);

          return EXIT_FAILURE;
        }
    }

  if(FbxConvert_printHelp)
    {
      fprintf(stdout,
              "Usage: %s [OPTION]... FILENAME\n"
              "\n"
              "      --help     display this help and exit\n"
              "      --version  display version information\n"
              "\n"
              "Report bugs to <morten.hustveit@gmail.com>\n", argv[0]);

      return EXIT_SUCCESS;
    }

  if (optind + 1 != argc)
    errx (EX_USAGE, "Usage: %s [OPTION]... FILENAME", argv[0]);

  *gFileName = argv[optind];

  // The first thing to do is to create the FBX SDK manager which is the
  // object allocator for almost all the classes in the SDK.
  if (!(FbxManager = FbxManager::Create()))
    errx (EXIT_FAILURE, "Unable to create the FBX SDK manager");

  // Load plugins from the executable directory
  FbxString lPath = FbxGetApplicationDirectory();
  FbxString lExtension = "so";

  FbxManager->LoadPluginsDirectory(lPath.Buffer(), lExtension.Buffer());

  // Create the entity that will hold the scene.
  scene = FbxScene::Create(FbxManager, "");

  if (!FbxManager)
    fprintf (stderr, "Unable to create the FBX SDK manager\n");

  fbxImporter = FbxImporter::Create(FbxManager, "");

  if (!FbxManager->GetIOPluginRegistry()->DetectReaderFileFormat(*gFileName, lFileFormat))
    lFileFormat = FbxManager->GetIOPluginRegistry()->FindReaderIDByDescription ("FBX binary (*.fbx)");;

  if(!fbxImporter->Initialize(gFileName->Buffer(), lFileFormat))
    errx (EXIT_FAILURE, "Unable to open `%s': %s", (const char *) *gFileName, (const char *) fbxImporter->GetLastErrorString ());

  if(!fbxImporter->Import(scene))
    errx (EXIT_FAILURE, "Unable to import `%s': %s", (const char *) *gFileName, (const char *) fbxImporter->GetLastErrorString ());

  // Convert Axis System to what is used in this example, if needed
  FbxAxisSystem OurAxisSystem (FbxAxisSystem::eYAxis, FbxAxisSystem::eParityOdd, FbxAxisSystem::eRightHanded);

  if (scene->GetGlobalSettings().GetSystemUnit().GetScaleFactor() != 100.0)
    FbxSystemUnit (100.0).ConvertScene (scene);

  if (scene->GetGlobalSettings().GetAxisSystem() != OurAxisSystem)
    OurAxisSystem.ConvertScene(scene);

  // Nurbs and patch attribute types are not supported yet.
  // Convert them into mesh node attributes to have them drawn.
  ConvertNurbsAndPatchRecursive(FbxManager, scene->GetRootNode());

  // Convert any .PC2 point cache data into the .MC format for
  // vertex cache deformer playback.
  PreparePointCacheData(scene);

  scene->FillAnimStackNameArray (takeNames);

  fbxImporter->Destroy();

  {
    FbxAMatrix lDummyGlobalPosition;

    int i, lCount = scene->GetRootNode()->GetChildCount();

    for (i = 0; i < lCount; i++)
      ExportMeshRecursive(scene->GetRootNode()->GetChild(i), lDummyGlobalPosition);
  }

  for (i = 0; i < takeNames.GetCount(); i++)
    {
      FbxTakeInfo* lCurrentTakeInfo;
      FbxTime period, start, stop, currentTime;

      FbxAnimStack *lCurrentAnimationStack;

      lCurrentAnimationStack = scene->FindMember(FBX_TYPE(FbxAnimStack), takeNames[i]->Buffer());

      if (lCurrentAnimationStack == NULL)
      {
        fprintf (stderr, "Animation stack %d not found!\n", i);

        return EXIT_FAILURE;
      }

      scene->GetEvaluator()->SetContext(lCurrentAnimationStack);

      lCurrentTakeInfo = scene->GetTakeInfo(*takeNames[i]);

      if (!lCurrentTakeInfo)
        continue;

      printf ("begin-take %s\n", (const char *) *takeNames[i]);

      period.SetMilliSeconds (10.0);

      start = lCurrentTakeInfo->mLocalTimeSpan.GetStart();
      stop = lCurrentTakeInfo->mLocalTimeSpan.GetStop();

      for (currentTime = start; currentTime <= stop; currentTime += period)
        ExportAnimScene (scene, currentTime);

      printf ("end-take\n");
    }

  return 0;
}

static void
ConvertNurbsAndPatchRecursive(FbxManager* pSdkManager, FbxNode* node)
{
  FbxNodeAttribute* lNodeAttribute;
  int i, count;

  lNodeAttribute = node->GetNodeAttribute();

  if (lNodeAttribute
      && (lNodeAttribute->GetAttributeType() == FbxNodeAttribute::eNurbs
          || lNodeAttribute->GetAttributeType() == FbxNodeAttribute::ePatch))
    {
      FbxGeometryConverter lConverter(pSdkManager);

      lConverter.TriangulateInPlace (node);
    }

  count = node->GetChildCount();

  for (i = 0; i < count; ++i)
    ConvertNurbsAndPatchRecursive(pSdkManager, node->GetChild(i));
}

static void
PreparePointCacheData(FbxScene* pScene)
{
  // This function show how to cycle thru scene elements in a linear way.
  int lIndex, lNodeCount;

  lNodeCount = FbxGetSrcCount<FbxNode>(pScene);

  for (lIndex = 0; lIndex < lNodeCount; lIndex++)
    {
      FbxNode* lNode;

      lNode = FbxGetSrc<FbxNode>(pScene, lIndex);

      if (lNode->GetGeometry())
        {
          int i, lVertexCacheDeformerCount;

          lVertexCacheDeformerCount = lNode->GetGeometry()->GetDeformerCount(FbxDeformer::eVertexCache);

          // There should be a maximum of 1 Vertex Cache Deformer for the moment
          lVertexCacheDeformerCount = lVertexCacheDeformerCount > 0 ? 1 : 0;

          for (i = 0; i < lVertexCacheDeformerCount; ++i)
            {
              FbxVertexCacheDeformer* lDeformer;

              lDeformer = static_cast<FbxVertexCacheDeformer*>(lNode->GetGeometry()->GetDeformer(i, FbxDeformer::eVertexCache));

              if(!lDeformer)
                continue;

              FbxCache* lCache = lDeformer->GetCache();

              if(!lCache)
                continue;

              // Process the point cache data only if the constraint is active
              if (!lDeformer->IsActive())
                continue;

              if (lCache->GetCacheFileFormat() == FbxCache::eMayaCache)
                {
                  if (!lCache->ConvertFromMCToPC2(FbxTime::GetFrameRate(pScene->GetGlobalSettings().GetTimeMode()), 0))
                    {
                      // Conversion failed, retrieve the error here
                      FbxString lTheErrorIs = lCache->GetError().GetLastErrorString();
                    }
                }

              // Now open the cache file to read from it
              if (!lCache->OpenFileForRead())
                {
                  // Cannot open file
                  FbxString lTheErrorIs = lCache->GetError().GetLastErrorString();

                  // Set the deformer inactive so we don't play it back
                  lDeformer->SetActive(false);
                }
            }
        }
    }
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

  int i, j;
  int lClusterCount=0;
  int lVertexCount = pMesh->GetControlPointsCount();
  int lSkinCount=pMesh->GetDeformerCount(FbxDeformer::eSkin);

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
  int lClusterCount = 0;
  int lSkinCount = 0;
  int lVertexCount;
  int i;

  std::map<vertex, int> vertexMap;
  std::vector<vertex> vertexArray;

  mesh = node->GetMesh ();
  lVertexCount = mesh->GetControlPointsCount();

  if (!lVertexCount)
    return;

  lSkinCount = mesh->GetDeformerCount(FbxDeformer::eSkin);

  for (int i = 0; i < lSkinCount; i++)
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
              const char *inputPath, *baseName;
              char *outputPath;

              if (!(lFileTexture = FbxCast<FbxFileTexture>(lProperty.GetSrcObject(FbxTexture::ClassId, lTextureIndex))))
                continue;

              inputPath = (const char *) lFileTexture->GetFileName ();

              if (0 != (baseName = strrchr (inputPath, '/')))
                ++baseName;
              else
                baseName = inputPath;

              if (-1 == asprintf (&outputPath, "Textures/%s", baseName))
                err (EXIT_FAILURE, "asprintf failed");

              if (-1 == mkdir ("Textures", 0777) && errno != EEXIST)
                err (EXIT_FAILURE, "Failed to create directory `Textures'");

              if (-1 == unlink (outputPath) && errno != ENOENT)
                err (EXIT_FAILURE, "Failed to unlink `%s'", outputPath);

              if (-1 == link (inputPath, outputPath))
                err (EXIT_FAILURE, "Failed to link `%s' to `%s'", inputPath, outputPath);

              printf ("texture %s\n", outputPath);

              free (outputPath);
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

  for (i = 0; i < mesh->GetPolygonCount(); i++)
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

static void
ExportMeshRecursive (FbxNode* node,
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
    ExportMeshRecursive(node->GetChild(i), lGlobalPosition);
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
ExportAnimScene(FbxScene* pScene, FbxTime& time)
{
  FbxAMatrix lDummyGlobalPosition;

  int i, lCount = pScene->GetRootNode()->GetChildCount();

  printf ("begin-frame\n");
  printf ("time %.3f\n", time.GetSecondDouble());

  for (i = 0; i < lCount; i++)
    ExportAnimRecursive(pScene->GetRootNode()->GetChild(i), time, lDummyGlobalPosition);

  printf ("end-frame\n");
}
