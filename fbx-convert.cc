#include <map>
#include <vector>

#include <err.h>
#include <getopt.h>
#include <math.h>
#include <sysexits.h>

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
PreparePointCacheData(KFbxScene* pScene);

static void
ExportMeshScene(KFbxScene* pScene);

static void
ExportAnimScene(KFbxScene* pScene, KTime& time);

static KFbxXMatrix
GetGeometry(KFbxNode* node);

static void
ConvertNurbsAndPatchRecursive(KFbxSdkManager* pSdkManager,
                              KFbxNode* node);

struct vertex
{
  float x, y, z;
  float u, v;

  bool
  operator<(const struct vertex &rhs) const
    {
#define TEST(a) if (a != rhs.a) return a < rhs.a;

      TEST(x)
      TEST(y)
      TEST(z)
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
  KString *gFileName = new KString;

  KFbxSdkManager* fbxSDKManager;
  KFbxImporter* fbxImporter;
  KFbxScene* scene;

  FbxArray<KString*> takeNames;

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
  if (!(fbxSDKManager = KFbxSdkManager::Create()))
    errx (EXIT_FAILURE, "Unable to create the FBX SDK manager");

  // Load plugins from the executable directory
  KString lPath = FbxGetApplicationDirectory();
  KString lExtension = "so";

  fbxSDKManager->LoadPluginsDirectory(lPath.Buffer(), lExtension.Buffer());

  // Create the entity that will hold the scene.
  scene = FbxScene::Create(fbxSDKManager, "");

  if (!fbxSDKManager)
    fprintf (stderr, "Unable to create the FBX SDK manager\n");

  fbxImporter = KFbxImporter::Create(fbxSDKManager, "");

  if (!fbxSDKManager->GetIOPluginRegistry()->DetectReaderFileFormat(*gFileName, lFileFormat))
    lFileFormat = fbxSDKManager->GetIOPluginRegistry()->FindReaderIDByDescription ("FBX binary (*.fbx)");;

  if(!fbxImporter->Initialize(gFileName->Buffer(), lFileFormat))
    errx (EXIT_FAILURE, "Unable to open `%s': %s", (const char *) *gFileName, (const char *) fbxImporter->GetLastErrorString ());

  if(!fbxImporter->Import(scene))
    errx (EXIT_FAILURE, "Unable to import `%s': %s", (const char *) *gFileName, (const char *) fbxImporter->GetLastErrorString ());

  // Convert Axis System to what is used in this example, if needed
  KFbxAxisSystem OurAxisSystem (KFbxAxisSystem::eYAxis, FbxAxisSystem::eParityOdd, FbxAxisSystem::eRightHanded);

  if (scene->GetGlobalSettings().GetAxisSystem() != OurAxisSystem)
    OurAxisSystem.ConvertScene(scene);

  if (scene->GetGlobalSettings().GetSystemUnit().GetScaleFactor() != 100.0)
    {
      FbxSystemUnit OurSystemUnit(100.0);
      OurSystemUnit.ConvertScene(scene);
    }

  // Nurbs and patch attribute types are not supported yet.
  // Convert them into mesh node attributes to have them drawn.
  ConvertNurbsAndPatchRecursive(fbxSDKManager, scene->GetRootNode());

  // Convert any .PC2 point cache data into the .MC format for
  // vertex cache deformer playback.
  PreparePointCacheData(scene);

  scene->FillAnimStackNameArray (takeNames);

  fbxImporter->Destroy();

  ExportMeshScene (scene);

  for (i = 0; i < takeNames.GetCount(); i++)
    {
      FbxTakeInfo* lCurrentTakeInfo;
      KTime period, start, stop, currentTime;

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
ConvertNurbsAndPatchRecursive(KFbxSdkManager* pSdkManager, KFbxNode* node)
{
  KFbxNodeAttribute* lNodeAttribute;
  int i, count;

  lNodeAttribute = node->GetNodeAttribute();

  if (lNodeAttribute
      && (lNodeAttribute->GetAttributeType() == KFbxNodeAttribute::eNurbs
          || lNodeAttribute->GetAttributeType() == KFbxNodeAttribute::ePatch))
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
              KFbxVertexCacheDeformer* lDeformer;

              lDeformer = static_cast<KFbxVertexCacheDeformer*>(lNode->GetGeometry()->GetDeformer(i, KFbxDeformer::eVertexCache));

              if(!lDeformer)
                continue;

              KFbxCache* lCache = lDeformer->GetCache();

              if(!lCache)
                continue;

              // Process the point cache data only if the constraint is active
              if (!lDeformer->IsActive())
                continue;

              if (lCache->GetCacheFileFormat() == KFbxCache::eMayaCache)
                {
                  if (!lCache->ConvertFromMCToPC2(KTime::GetFrameRate(pScene->GetGlobalSettings().GetTimeMode()), 0))
                    {
                      // Conversion failed, retrieve the error here
                      KString lTheErrorIs = lCache->GetError().GetLastErrorString();
                    }
                }

              // Now open the cache file to read from it
              if (!lCache->OpenFileForRead())
                {
                  // Cannot open file
                  KString lTheErrorIs = lCache->GetError().GetLastErrorString();

                  // Set the deformer inactive so we don't play it back
                  lDeformer->SetActive(false);
                }
            }
        }
    }
}

// Get the geometry deformation local to a node. It is never inherited by the
// children.
KFbxXMatrix GetGeometry(KFbxNode* node)
{
  KFbxXMatrix lGeometry;

  lGeometry.SetT(node->GetGeometricTranslation(KFbxNode::eSourcePivot));
  lGeometry.SetR(node->GetGeometricRotation(KFbxNode::eSourcePivot));
  lGeometry.SetS(node->GetGeometricScaling(KFbxNode::eSourcePivot));

  return lGeometry;
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
ExportClusterDeformation(KFbxXMatrix& pGlobalPosition,
                         KFbxMesh* pMesh)
{
    KFbxCluster::ELinkMode lClusterMode = ((KFbxSkin*)pMesh->GetDeformer(0, KFbxDeformer::eSkin))->GetCluster(0)->GetLinkMode();

    int i, j;
    int lClusterCount=0;
    int lVertexCount = pMesh->GetControlPointsCount();
    int lSkinCount=pMesh->GetDeformerCount(KFbxDeformer::eSkin);

    VertexWeights *weights;

    weights = new VertexWeights[lVertexCount];

    for(i=0; i<lSkinCount; ++i)
      {
        lClusterCount = ((KFbxSkin *)pMesh->GetDeformer(i, KFbxDeformer::eSkin))->GetClusterCount();

        for (j=0; j<lClusterCount; ++j)
          {
            KFbxCluster* lCluster = ((KFbxSkin *) pMesh->GetDeformer(i, KFbxDeformer::eSkin))->GetCluster(j);
            int k;

            if (!lCluster->GetLink())
              continue;

            KFbxXMatrix lReferenceGlobalInitPosition;
            KFbxXMatrix lClusterGlobalInitPosition;
            KFbxXMatrix lReferenceGeometry;
            KFbxXMatrix lClusterGeometry;

            KFbxXMatrix lClusterRelativeInitPosition;
            KFbxXMatrix lVertexTransformMatrix;

            if (lClusterMode == FbxCluster::eAdditive && lCluster->GetAssociateModel())
              {
                lCluster->GetTransformAssociateModelMatrix(lReferenceGlobalInitPosition);
              }
            else
              {
                lCluster->GetTransformMatrix(lReferenceGlobalInitPosition);

                lReferenceGeometry = GetGeometry(pMesh->GetNode());
                lReferenceGlobalInitPosition *= lReferenceGeometry;
              }

            lCluster->GetTransformLinkMatrix(lClusterGlobalInitPosition);

            lClusterRelativeInitPosition = lClusterGlobalInitPosition.Inverse() * lReferenceGlobalInitPosition;

            printf ("bone-matrix %d", j);

            for (k = 0; k < 16; ++k)
              printf (" %.6g", lClusterRelativeInitPosition.Get (k / 4, k % 4));

            printf ("\n");

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

    for (i = 0; i < lVertexCount; i++)
      {
        printf ("bone-weights %d %.6g %.6g %.6g %.6g %u %u %u %u\n",
                i,
                weights[i].weights[0], weights[i].weights[1], weights[i].weights[2], weights[i].weights[3],
                weights[i].bones[0], weights[i].bones[1], weights[i].bones[2], weights[i].bones[3]);
      }

    delete [] weights;
}

static void
ExportMesh(KFbxNode* node, KFbxXMatrix& pGlobalPosition)
{
  KFbxMesh* mesh;
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

  lSkinCount = mesh->GetDeformerCount(KFbxDeformer::eSkin);

  for (int i = 0; i < lSkinCount; i++)
    lClusterCount += ((KFbxSkin *)(mesh->GetDeformer(i, KFbxDeformer::eSkin)))->GetClusterCount();

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

              printf ("texture %s\n", (const char *) lFileTexture->GetFileName ());
            }
        }
    }

  printf ("matrix");
  for (i = 0; i < 16; ++i)
    printf (" %.6g", pGlobalPosition.Get (i / 4, i % 4));
  printf ("\n");

  if (lClusterCount)
    ExportClusterDeformation (pGlobalPosition, mesh);

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

          double *xyz;
          FbxVector2 uv;

          xyz = (double *) mesh->GetControlPoints()[mesh->GetPolygonVertex(i, j)];

          newVertex.x = xyz[0];
          newVertex.y = xyz[1];
          newVertex.z = xyz[2];

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

      v = vertexArray[i];

      printf ("xyz %d %.6g %.6g %.6g\n", i, v.x, v.y, v.z);
      printf ("uv %d %.6g %.6g\n", i, v.u, v.v);
    }

  printf ("end-mesh\n");

  fflush (stdout);
}

static void
ExportMeshRecursive(KFbxNode* node,
                    KFbxXMatrix& pParentGlobalPosition)
{
  KTime time;

  // Compute the node's global position.
  KFbxXMatrix lGlobalPosition = node->EvaluateGlobalTransform(time);

  // Geometry offset.
  // it is not inherited by the children.
  KFbxXMatrix lGeometryOffset = GetGeometry(node);
  KFbxXMatrix lGlobalOffPosition = lGlobalPosition * lGeometryOffset;

  KFbxNodeAttribute* lNodeAttribute = node->GetNodeAttribute();

  if (lNodeAttribute && lNodeAttribute->GetAttributeType() == KFbxNodeAttribute::eMesh)
    ExportMesh(node, lGlobalPosition);

  int i, lCount = node->GetChildCount();

  for (i = 0; i < lCount; i++)
    ExportMeshRecursive(node->GetChild(i), lGlobalPosition);
}

static void
ExportMeshScene(KFbxScene* pScene)
{
  KFbxXMatrix lDummyGlobalPosition;

  int i, lCount = pScene->GetRootNode()->GetChildCount();

  for (i = 0; i < lCount; i++)
    ExportMeshRecursive(pScene->GetRootNode()->GetChild(i), lDummyGlobalPosition);
}

static void ExportClusterDeformation(KFbxXMatrix& pGlobalPosition,
                              KFbxMesh* pMesh,
                              KTime& time)
{
  KFbxCluster::ELinkMode lClusterMode;
  KFbxSkin *skin;

  skin = (KFbxSkin *) pMesh->GetDeformer(0, KFbxDeformer::eSkin);

  if (!skin)
    return;

  lClusterMode = skin->GetCluster(0)->GetLinkMode();

  int i, j;
  int lClusterCount=0;
  int lVertexCount = pMesh->GetControlPointsCount();
  int lSkinCount=pMesh->GetDeformerCount(KFbxDeformer::eSkin);

  if (!lVertexCount)
    return;

  for(i=0; i<lSkinCount; ++i)
    {
      lClusterCount =( (KFbxSkin *)pMesh->GetDeformer(i, KFbxDeformer::eSkin))->GetClusterCount();

      for (j=0; j<lClusterCount; ++j)
        {
          KFbxCluster* lCluster =((KFbxSkin *) pMesh->GetDeformer(i, KFbxDeformer::eSkin))->GetCluster(j);
          int k;

          if (!lCluster->GetLink())
            continue;

          KFbxXMatrix lReferenceGlobalCurrentPosition;
          KFbxXMatrix lClusterGlobalInitPosition;
          KFbxXMatrix lClusterGlobalCurrentPosition;
          KFbxXMatrix lReferenceGeometry;
          KFbxXMatrix lClusterGeometry;

          KFbxXMatrix lClusterRelativeInitPosition;
          KFbxXMatrix lClusterRelativeCurrentPositionInverse;
          KFbxXMatrix lVertexTransformMatrix;

          if (lClusterMode == FbxCluster::eAdditive && lCluster->GetAssociateModel())
            {
              lReferenceGlobalCurrentPosition = lCluster->GetAssociateModel()->EvaluateGlobalTransform(time);

              // Geometric transform of the model
              lReferenceGeometry = GetGeometry(lCluster->GetAssociateModel());
              lReferenceGlobalCurrentPosition *= lReferenceGeometry;
            }
          else
            lReferenceGlobalCurrentPosition = pGlobalPosition;


          lClusterGlobalCurrentPosition = lCluster->GetLink()->EvaluateGlobalTransform(time);

          // Compute the current position of the link relative to the reference.
          lClusterRelativeCurrentPositionInverse = lReferenceGlobalCurrentPosition.Inverse() * lClusterGlobalCurrentPosition;

#if 0
          // Compute the shift of the link relative to the reference.
          lVertexTransformMatrix = lClusterRelativeCurrentPositionInverse * lClusterRelativeInitPosition;
#endif

          printf ("bone-matrix %d", j);

          for (k = 0; k < 16; ++k)
            printf (" %.6g", lClusterRelativeCurrentPositionInverse.Get (k / 4, k % 4));

          printf ("\n");
        }
    }
}

static void
ExportAnimRecursive(KFbxNode* node,
                    KTime& time,
                    KFbxXMatrix& pParentGlobalPosition)
{
  KFbxXMatrix lGlobalPosition, lGeometryOffset, lGlobalOffPosition;
  KFbxNodeAttribute* lNodeAttribute;
  int i, count;

  lGlobalPosition = node->EvaluateGlobalTransform(time);
  lGeometryOffset = GetGeometry(node);
  lGlobalOffPosition = lGlobalPosition * lGeometryOffset;

  lNodeAttribute = node->GetNodeAttribute();

  if (lNodeAttribute
      && lNodeAttribute->GetAttributeType() == KFbxNodeAttribute::eMesh)
    {
      ExportClusterDeformation (lGlobalPosition, (KFbxMesh *) node->GetNodeAttribute(), time);
    }

  count = node->GetChildCount();

  for (i = 0; i < count; ++i)
    ExportAnimRecursive(node->GetChild(i), time, lGlobalPosition);
}

static void
ExportAnimScene(KFbxScene* pScene, KTime& time)
{
  KFbxXMatrix lDummyGlobalPosition;

  int i, lCount = pScene->GetRootNode()->GetChildCount();

  printf ("begin-frame\n");
  printf ("time %.3f\n", time.GetSecondDouble());

  for (i = 0; i < lCount; i++)
    ExportAnimRecursive(pScene->GetRootNode()->GetChild(i), time, lDummyGlobalPosition);

  printf ("end-frame\n");
}
