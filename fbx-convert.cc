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

#include <PVRTTriStrip.h>

#include "fbx-convert.h"

static int FbxConvert_printHelp;
static int FbxConvert_printVersion;
static const char *FbxConvert_format = "binary";
static unsigned int FbxConvert_pointerSize = 32;

static struct option FbxConvert_longOptions[] =
{
    { "format", required_argument, 0, 'f' },
    { "pointer-size", required_argument, 0, 'p' },
    { "help",     no_argument, &FbxConvert_printHelp, 1 },
    { "version",  no_argument, &FbxConvert_printVersion, 1 },
    { 0, 0, 0, 0 }
};

static void
PreparePointCacheData(FbxScene* pScene);

static void
ConvertNurbsAndPatchRecursive(FbxManager* pSdkManager,
                              FbxNode* node);

void
ConvertMeshToIntermediateRecursive (fbx_model &output,
                                    FbxNode* node,
                                    const FbxAMatrix& pParentGlobalPosition);

void
ConvertTakeToIntermediate (fbx_model &output, FbxScene *scene, FbxString *takeName);

static FbxAMatrix
GetGeometry(FbxNode* node);

static void
GenTriStrips (fbx_model &model);

static void
BiasUVCoordinates (fbx_model &model);

static void
FindBoundingBox (fbx_model &model);

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
        case 0:

          break;

        case 'f':

          FbxConvert_format = optarg;

          break;

        case 'p':

          FbxConvert_pointerSize = atoi (optarg);

          break;

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
              "      --format\n"
              "      --pointer-size\n"
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

  fbx_model model;

  for (i = 0; i < scene->GetRootNode()->GetChildCount(); i++)
    ConvertMeshToIntermediateRecursive (model, scene->GetRootNode()->GetChild(i), FbxAMatrix ());

  for (i = 0; i < takeNames.GetCount(); i++)
    ConvertTakeToIntermediate (model, scene, takeNames[i]);

  GenTriStrips (model);
  BiasUVCoordinates (model);
  FindBoundingBox (model);

  if (!strcmp (FbxConvert_format, "binary"))
    FbxConvertExportBinary (model, FbxConvert_pointerSize);
  else if (!strcmp (FbxConvert_format, "json"))
    FbxConvertExportJSON (model);
  else
    {
      fprintf (stderr, "Unknown format: %s\n", FbxConvert_format);

      return EXIT_FAILURE;
    }

  return EXIT_SUCCESS;
}

static void
GenTriStrips (fbx_model &model)
{
  for (auto &mesh : model.meshes)
    {
      std::vector<size_t> remap;
      std::vector<float> new_xyz;
      std::vector<float> new_uv;
      std::vector<uint8_t> new_weights;
      std::vector<uint8_t> new_bones;
      size_t fill = 0;

      PVRTTriStripList (&mesh.indices[0], mesh.indices.size () / 3);

      remap.resize (mesh.xyz.size () / 3, (size_t) -1);

      for (auto &i : mesh.indices)
        {
          if (remap[i] == (size_t) -1)
            {
              remap[i] = fill++;

              new_xyz.push_back (mesh.xyz[i * 3 + 0]);
              new_xyz.push_back (mesh.xyz[i * 3 + 1]);
              new_xyz.push_back (mesh.xyz[i * 3 + 2]);
              new_uv.push_back (mesh.uv[i * 2 + 0]);
              new_uv.push_back (mesh.uv[i * 2 + 1]);

              if (!mesh.weights.empty ())
                {
                  new_weights.push_back (mesh.weights[i * 4 + 0]);
                  new_weights.push_back (mesh.weights[i * 4 + 1]);
                  new_weights.push_back (mesh.weights[i * 4 + 2]);
                  new_weights.push_back (mesh.weights[i * 4 + 3]);
                  new_bones.push_back (mesh.bones[i * 4 + 0]);
                  new_bones.push_back (mesh.bones[i * 4 + 1]);
                  new_bones.push_back (mesh.bones[i * 4 + 2]);
                  new_bones.push_back (mesh.bones[i * 4 + 3]);
                }
            }

          i = remap[i];
        }

      mesh.xyz.swap (new_xyz);
      mesh.uv.swap (new_uv);
      mesh.weights.swap (new_weights);
      mesh.bones.swap (new_bones);
    }
}

static void
BiasUVCoordinates (fbx_model &model)
{
  /* Ensure all UV coordinates are above 0 */

  for (auto &mesh : model.meshes)
    {
      float min_u = 0.0f, min_v = 0.0f;
      size_t i;

      for (i = 0; i < mesh.uv.size (); i += 2)
        {
          if (mesh.uv[i] < min_u)
            min_u = mesh.uv[i];

          if (mesh.uv[i + 1] < min_v)
            min_v = mesh.uv[i + 1];
        }

      min_u = floor (min_u);
      min_v = floor (min_v);

      if (!min_u && !min_v)
        continue;

      fprintf (stderr, "Bias %.2f %.2f\n", min_u, min_v);

      for (i = 0; i < mesh.uv.size (); i += 2)
        {
          mesh.uv[i] -= min_u;
          mesh.uv[i] -= min_v;
        }
    }
}

static void
FindBoundingBox (fbx_model &model)
{
  for (auto &mesh : model.meshes)
    {
      float minX = 0, minY = 0, minZ = 0;
      float maxX = 0, maxY = 0, maxZ = 0;

      if (mesh.xyz.empty ())
        continue;

      minX = maxX = mesh.xyz[0];
      minY = maxY = mesh.xyz[1];
      minZ = maxZ = mesh.xyz[2];

      for (size_t i = 3; i < mesh.xyz.size (); i += 3)
        {
          if (mesh.xyz[i] < minX)
            minX = mesh.xyz[i];
          else if (mesh.xyz[i] > maxX)
            maxX = mesh.xyz[i];

          if (mesh.xyz[i + 1] < minY)
            minY = mesh.xyz[i + 1];
          else if (mesh.xyz[i + 1] > maxY)
            maxY = mesh.xyz[i + 1];

          if (mesh.xyz[i + 2] < minZ)
            minZ = mesh.xyz[i + 2];
          else if (mesh.xyz[i + 2] > maxZ)
            maxZ = mesh.xyz[i + 2];
        }

      mesh.boundsMin.v[0] = minX;
      mesh.boundsMin.v[1] = minY;
      mesh.boundsMin.v[2] = minZ;
      mesh.boundsMax.v[0] = maxX;
      mesh.boundsMax.v[1] = maxY;
      mesh.boundsMax.v[2] = maxZ;
    }
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

struct tmp_vertex
{
  unsigned int controlPoint;
  float u, v;

  bool
  operator<(const struct tmp_vertex &rhs) const
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
ConvertInitialClusterDeformationToIntermediate (fbx_mesh &output,
                                                FbxAMatrix& pGlobalPosition,
                                                FbxMesh* pMesh,
                                                const std::vector<tmp_vertex> &vertexArray)
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

          for (k = 0; k < 16; ++k)
            output.bindPose.push_back (lClusterRelativeInitPosition.Get (k / 4, k % 4));

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
      unsigned int uintWeights[4];

      controlPoint = vertexArray[i].controlPoint;

      uintWeights[3] = weights[controlPoint].weights[3] * 255;
      uintWeights[2] = weights[controlPoint].weights[2] * 255;
      uintWeights[1] = weights[controlPoint].weights[1] * 255;
      uintWeights[0] = 255 - (uintWeights[1] + uintWeights[2] + uintWeights[3]);

      output.weights.push_back (uintWeights[0]);
      output.weights.push_back (uintWeights[1]);
      output.weights.push_back (uintWeights[2]);
      output.weights.push_back (uintWeights[3]);
      output.bones.push_back (weights[controlPoint].bones[0]);
      output.bones.push_back (weights[controlPoint].bones[1]);
      output.bones.push_back (weights[controlPoint].bones[2]);
      output.bones.push_back (weights[controlPoint].bones[3]);
    }

  delete [] weights;
}

static void
ExtractUserProperties (fbx_model &output, const char *properties)
{
  char *buffer;
  char *line, *line_saveptr, *end, *comma;

  buffer = strdup (properties);

  line = strtok_r (buffer, "\n", &line_saveptr);

  while (line)
    {
      end = strchr (line, 0);

      while (end > line && isspace (end[-1]))
        *--end = 0;

      if (NULL != (comma = strchr (line, ',')))
        {
          int start, end;
          int prefix;

          *comma++ = 0;

          while (*comma && isspace (*comma))
            ++comma;

          for (prefix = 0; line[prefix]; ++prefix)
            {
              if (line[prefix] != comma[prefix])
                break;
            }

          if (prefix <= 1 || line[prefix - 1] != '_')
            continue;

          --prefix;

          if (   1 == sscanf (line + prefix,  "_start=%d", &start)
              && 1 == sscanf (comma + prefix, "_end=%d",   &end))
            {
              fbx_take_range ftr;

              line[prefix] = 0;

              ftr.name = line;
              ftr.begin = start * 2;
              ftr.end = end * 2;

              output.takeRanges.push_back (ftr);
            }
        }

      line = strtok_r (NULL, "\n", &line_saveptr);
    }

  free (buffer);
}

void
ConvertMeshToIntermediateRecursive (fbx_model &output,
                                    FbxNode* node,
                                    const FbxAMatrix& pParentGlobalPosition)
{
  FbxTime time;

  // Compute the node's global position.
  FbxAMatrix lGlobalPosition = node->EvaluateGlobalTransform(time);

  // Geometry offset.
  // it is not inherited by the children.
  FbxAMatrix lGeometryOffset = GetGeometry(node);
  FbxAMatrix lGlobalOffPosition = lGlobalPosition * lGeometryOffset;

  FbxNodeAttribute* lNodeAttribute = node->GetNodeAttribute();

  if (lNodeAttribute)
    {
      switch (lNodeAttribute->GetAttributeType())
        {
        case FbxNodeAttribute::eMesh:

            {
              FbxMesh* mesh;
              unsigned int lClusterCount = 0;
              unsigned int lSkinCount = 0;
              unsigned int lVertexCount;
              unsigned int i;
              const char *name;

              std::map<tmp_vertex, int> vertexMap;
              std::vector<tmp_vertex> vertexArray;

              mesh = node->GetMesh ();
              lVertexCount = mesh->GetControlPointsCount();

              name = node->GetName ();

              if (!strncmp (name, "Bn_", 3))
                return;

              FbxProperty prop = node->FindProperty("UDP3DSMAX", false);

              if(prop.IsValid())
                ExtractUserProperties (output, prop.Get<FbxString>());

              if (!lVertexCount)
                return;

              lSkinCount = mesh->GetDeformerCount(FbxDeformer::eSkin);

              for (i = 0; i < lSkinCount; i++)
                lClusterCount += ((FbxSkin *)(mesh->GetDeformer(i, FbxDeformer::eSkin)))->GetClusterCount();

              output.meshes.push_back (fbx_mesh ());
              fbx_mesh &newMesh = output.meshes.back ();

              if (!strncasecmp (name, "LOD_", 4))
                {
                  char *endptr;
                  newMesh.lod = strtod (name + 4, &endptr);

                  if (*endptr)
                    newMesh.lod = HUGE_VAL;
                }
              else
                newMesh.lod = HUGE_VAL;

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

                          newMesh.diffuseTexture = (const char *) lFileTexture->GetRelativeFileName ();
                        }
                    }
                }

              for (i = 0; i < 16; ++i)
                newMesh.matrix.v[i] = lGlobalPosition.Get (i / 4, i % 4);

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
                      tmp_vertex newVertex;

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
                      newMesh.indices.push_back (indices[0]);
                      newMesh.indices.push_back (indices[j - 1]);
                      newMesh.indices.push_back (indices[j]);
                    }

                  delete [] indices;
                }

              for (i = 0; i < vertexArray.size (); ++i)
                {
                  tmp_vertex v;
                  double *xyz;

                  v = vertexArray[i];

                  xyz = (double *) mesh->GetControlPoints()[v.controlPoint];

                  newMesh.xyz.push_back (xyz[0]);
                  newMesh.xyz.push_back (xyz[1]);
                  newMesh.xyz.push_back (xyz[2]);
                  newMesh.uv.push_back (v.u);
                  newMesh.uv.push_back (v.v);
                }

              if (lClusterCount)
                ConvertInitialClusterDeformationToIntermediate (newMesh, lGlobalPosition, mesh, vertexArray);
            }

          break;

        default:

          break;
        }
    }

  int i, lCount = node->GetChildCount();

  for (i = 0; i < lCount; i++)
    ConvertMeshToIntermediateRecursive (output, node->GetChild(i), lGlobalPosition);
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

static void ExportClusterDeformationAtTime(fbx_frame &output,
                                           FbxAMatrix& pGlobalPosition,
                                           FbxMesh* pMesh,
                                           FbxTime& time)
{
  FbxCluster::ELinkMode lClusterMode;
  FbxSkin *skin;

  skin = (FbxSkin *) pMesh->GetDeformer(0, FbxDeformer::eSkin);

  if (!skin)
    return;

  lClusterMode = skin->GetCluster(0)->GetLinkMode();

  int i, j, k;
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

          if (0 != strcmp (FbxConvert_format, "json"))
            {
              FbxQuaternion quat;
              FbxVector4 transl;

              transl = lClusterRelativeCurrentPositionInverse.GetT ();
              quat = lClusterRelativeCurrentPositionInverse.GetQ ();

              output.pose.push_back (quat[0]);
              output.pose.push_back (quat[1]);
              output.pose.push_back (quat[2]);
              output.pose.push_back (quat[3]);
              output.pose.push_back (transl[0]);
              output.pose.push_back (transl[1]);
              output.pose.push_back (transl[2]);
            }
          else
            {
              for (k = 0; k < 16; ++k)
                output.pose.push_back (lClusterRelativeCurrentPositionInverse.Get (k / 4, k % 4));
            }
        }
    }
}

static void
ConvertFrameToIntermediate (fbx_frame &output,
                            FbxNode* node,
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
      if (strncmp (node->GetName (), "Bn_", 3))
        ExportClusterDeformationAtTime (output, lGlobalPosition, (FbxMesh *) node->GetNodeAttribute(), time);
    }

  count = node->GetChildCount();

  for (i = 0; i < count; ++i)
    ConvertFrameToIntermediate (output, node->GetChild(i), time, lGlobalPosition);
}

void
ConvertTakeToIntermediate (fbx_model &output, FbxScene *scene, FbxString *takeName)
{
  FbxTakeInfo* lCurrentTakeInfo;
  FbxTime period, currentTime;

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

  period.SetMilliSeconds (1000. / 60.);

  if (output.takeRanges.empty ())
    {
      FbxAMatrix lDummyGlobalPosition;

      FbxTime start, stop;
      output.takes.push_back (fbx_take ());
      fbx_take &take = output.takes.back ();

      take.name = (const char *) *takeName;
      take.interval = 1000. / 60.;

      start = lCurrentTakeInfo->mLocalTimeSpan.GetStart();
      stop = lCurrentTakeInfo->mLocalTimeSpan.GetStop();

      for (currentTime = start; currentTime <= stop; currentTime += period)
        {
          int i, lCount = scene->GetRootNode()->GetChildCount();

          take.frames.push_back (fbx_frame ());
          fbx_frame &frame = take.frames.back ();

          for (i = 0; i < lCount; i++)
            ConvertFrameToIntermediate (frame, scene->GetRootNode()->GetChild(i), currentTime, lDummyGlobalPosition);
        }
    }
  else
    {
      FbxAMatrix lDummyGlobalPosition;

      for (auto &range : output.takeRanges)
        {
          unsigned int i;

          output.takes.push_back (fbx_take ());
          fbx_take &take = output.takes.back ();

          take.name = range.name;
          take.interval = 1000. / 60.;

          for (i = range.begin; i < range.end; ++i, currentTime += period)
            {
              int j, lCount = scene->GetRootNode()->GetChildCount();

              currentTime.SetFrame (i, FbxTime::eFrames60);

              take.frames.push_back (fbx_frame ());
              fbx_frame &frame = take.frames.back ();

              for (j = 0; j < lCount; j++)
                ConvertFrameToIntermediate (frame, scene->GetRootNode()->GetChild(j), currentTime, lDummyGlobalPosition);
            }
        }
    }
}
